#include "core/application.h"

#include <chrono>
#include <memory>

#include "render/passes/aov_pass.h"
#include "render/passes/auto_exposure_pass.h"
#include "render/passes/bloom_pass.h"
#include "render/passes/denoiser_pass.h"
#include "render/passes/taa_pass.h"
#include "render/passes/tonemap_pass.h"
#include "core/gl_debug.h"
#include "render/gui.h"
#include "render/passes/gui_pass.h"
#include "core/log.h"
#include "render/passes/path_tracer_pass.h"
#include "render/passes/raster_gbuffer_pass.h"
#include "render/render_pass.h"
#include "render/renderer.h"
#include "render/passes/restir_pass.h"
#include "gpu/texture.h"

static const std::filesystem::path denoiserShaderPath = "shader/denoiser.comp";
static const std::filesystem::path gbufferVertPath = "shader/gbuffer.vert";
static const std::filesystem::path gbufferFragPath = "shader/gbuffer.frag";

Application::Application(Scene initialScene)
    : scene(std::move(initialScene)), camera(this->scene.cameraSettings), window(camera.image_width, camera.image_height, this->scene.name.c_str()),
      renderer(camera.image_width, camera.image_height), sceneEntries(sceneRegistry()),
      timeSeed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())) {
    Log::info("OpenGL version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    Log::info("Image dimensions: {} x {}", camera.image_width, camera.image_height);

    // GLDebug::enable();

    Gui::init(window);

    for (size_t i = 0; i < sceneEntries.size(); ++i) {
        if (sceneEntries[i].name == scene.name) {
            sceneSwitch.current = static_cast<int>(i);
            break;
        }
    }

    Log::info("Adding render passes");
    renderer.addRenderPass(std::make_unique<RasterGBufferPass>(gbufferVertPath, gbufferFragPath));
    renderer.addRenderPass(std::make_unique<RestirPass>(camera.image_width, camera.image_height));
    renderer.addRenderPass(std::make_unique<PathTracerPass>(camera.image_width, camera.image_height));
    renderer.addRenderPass(std::make_unique<DenoiserPass>(denoiserShaderPath));
    renderer.addRenderPass(std::make_unique<BloomPass>(camera.image_width, camera.image_height, settings));
    renderer.addRenderPass(std::make_unique<AutoExposurePass>(camera.image_width, camera.image_height, settings));
    renderer.addRenderPass(std::make_unique<TonemapPass>(camera.image_width, camera.image_height));
    // TAA runs after tonemap so its history stays in perceptual/LDR space (firefly-safe)
    // and before AOV so debug AOV overrides don't poison the TAA history.
    renderer.addRenderPass(std::make_unique<TaaPass>(camera.image_width, camera.image_height));
    renderer.addRenderPass(std::make_unique<AovPass>(camera.image_width, camera.image_height, settings));

    renderer.addRenderPass(
        std::make_unique<GuiPass>(fpsTimer, gpuTimer, renderer.getPassTimings(), sceneEntries, sceneSwitch, settings)); // keep last

    renderer.loadScene(scene, camera);
}

int Application::run() {
    fpsTimer.start();
    while (!window.shouldClose()) {

        Gui::beginFrame();

        if (window.pendingResize) {
            window.pendingResize = false;
            window.width = window.pendingWidth;
            window.height = window.pendingHeight;
            camera.resize(window.width, window.height);
            renderer.resize(window.width, window.height);
            frameIndex = 0;
        }

        const InputState input = window.pollInput();
        camera.update(input, fpsTimer.deltaTime);

        if (camera.moving) {
            frameIndex = 0;
            camera.moving = false;
        }

        RenderContext ctx{scene, camera, ++frameIndex, timeSeed++, static_cast<float>(fpsTimer.deltaTime)};

        // Sub-pixel jitter for AA. Re-uploads the camera UBO every frame because
        // the projection matrix changes each frame; jitter resets implicitly when
        // frameIndex resets after camera motion.
        camera.applyJitter(ctx.frameIndex);
        renderer.updateCameraUbo(camera);
        if (renderer.reloadShadersIfChanged(ctx)) {
            // Shader reloads discard the history too — the program being reloaded
            // may have changed the meaning of the data we've been accumulating.
            frameIndex = 0;
            ctx.frameIndex = 0;
        }

        gpuTimer.start();
        renderer.render(ctx);
        gpuTimer.end();

        window.getFrameBufferSize();
        if (input.debugGBufferNormal) {
            renderer.blitGBufferAttachmentToSwapChain(GBuffer::ATTACH_NORMAL, window.width, window.height);
        } else if (input.debugGBufferPosition) {
            renderer.blitGBufferAttachmentToSwapChain(GBuffer::ATTACH_POS_MATID, window.width, window.height);
        } else {
            renderer.blitToSwapChain(window.width, window.height);
        }

        Gui::endFrame();

        window.pollEvents();
        window.swapBuffers();

        fpsTimer.end();

        applyPendingSceneSwitch();
    }

    return 0;
}

void Application::applyPendingSceneSwitch() {
    if (sceneSwitch.requested < 0 || sceneSwitch.requested == sceneSwitch.current) {
        return;
    }

    const int idx = sceneSwitch.requested;
    Log::info("Switching scene to '{}'", sceneEntries[idx].name);

    scene = sceneEntries[idx].factory();
    camera = Camera(scene.cameraSettings);
    // Camera() is built from the scene's *authored* resolution, so it discards any resize the
    // window has seen since startup. Re-apply the live framebuffer size before loadScene(),
    // which fires uploadUniforms() on every pass: DenoiserPass takes its `image_size` from
    // camera.image_width/height there, and the camera UBO takes its projection — and hence
    // aspect ratio — from this object. Render targets are already at the window size and are
    // deliberately not reallocated here.
    camera.resize(window.width, window.height);
    renderer.loadScene(scene, camera);
    window.setTitle(scene.name);
    frameIndex = 0;

    sceneSwitch.current = idx;
    sceneSwitch.requested = -1;
}

Application::~Application() {
    Gui::shutdown();
}
