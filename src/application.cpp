#include "application.h"

#include <memory>

#include "denoiser_pass.h"
#include "gl_debug.h"
#include "gui.h"
#include "gui_pass.h"
#include "log.h"
#include "path_tracer_pass.h"
#include "raster_gbuffer_pass.h"
#include "render_pass.h"
#include "renderer.h"
#include "texture.h"

static const std::filesystem::path denoiserShaderPath = "shader/denoiser.comp";
static const std::filesystem::path gbufferVertPath = "shader/gbuffer.vert";
static const std::filesystem::path gbufferFragPath = "shader/gbuffer.frag";

Application::Application(Scene initialScene)
    : scene(std::move(initialScene)), camera(this->scene.cameraSettings), window(camera.image_width, camera.image_height, this->scene.name.c_str()),
      renderer(camera.image_width, camera.image_height), sceneEntries(sceneRegistry()) {
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
    renderer.addRenderPass(std::make_unique<PathTracerPass>(camera.image_width, camera.image_height));
    renderer.addRenderPass(std::make_unique<DenoiserPass>(denoiserShaderPath));

    renderer.addRenderPass(std::make_unique<GuiPass>(fpsTimer, gpuTimer, sceneEntries, sceneSwitch)); // keep last

    renderer.loadScene(scene, camera);
}

int Application::run() {
    while (!window.shouldClose()) {

        Gui::beginFrame();

        const InputState input = window.pollInput();
        camera.update(input, fpsTimer.deltaTime);

        if (camera.moving) {
            renderer.updateCameraUbo(camera);
            frameIndex = 0;
            camera.moving = false;
        }

        RenderContext ctx{scene, camera, ++frameIndex, timeSeed++};
        if (renderer.reloadShadersIfChanged(ctx)) {
            // Shader reloads discard the history too — the program being reloaded
            // may have changed the meaning of the data we've been accumulating.
            frameIndex = 0;
            ctx.frameIndex = 0;
        }

        gpuTimer.start();
        Texture& output = renderer.render(ctx);
        gpuTimer.end();

        window.getFrameBufferSize();
        if (input.debugGBufferNormal) {
            renderer.blitGBufferAttachmentToSwapChain(GBuffer::ATTACH_NORMAL, window.width, window.height);
        } else if (input.debugGBufferPosition) {
            renderer.blitGBufferAttachmentToSwapChain(GBuffer::ATTACH_POS_MATID, window.width, window.height);
        } else {
            renderer.blitToSwapChain(output, window.width, window.height);
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
    renderer.loadScene(scene, camera);
    window.setTitle(scene.name);
    frameIndex = 0;

    sceneSwitch.current = idx;
    sceneSwitch.requested = -1;
}

Application::~Application() {
    Gui::shutdown();
}
