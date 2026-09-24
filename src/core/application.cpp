#include "core/application.h"

#include <chrono>
#include <exception>
#include <memory>

#include "render/passes/aov_pass.h"
#include "render/passes/auto_exposure_pass.h"
#include "render/passes/bloom_pass.h"
#include "render/passes/denoiser_pass.h"
#include "render/passes/taa_pass.h"
#include "render/passes/tonemap_pass.h"
#include "gpu/gl_debug.h"
#include "render/gui.h"
#include "core/log.h"
#include "render/passes/path_tracer_pass.h"
#include "render/passes/raster_gbuffer_pass.h"
#include "render/render_pass.h"
#include "render/renderer.h"
#include "render/passes/restir_pass.h"

static const std::filesystem::path denoiserShaderPath = "shader/denoiser.comp";
static const std::filesystem::path gbufferVertPath = "shader/gbuffer.vert";
static const std::filesystem::path gbufferFragPath = "shader/gbuffer.frag";

Application::Application(Scene initialScene)
    : scene(std::move(initialScene)), camera(this->scene.cameraSettings), window(camera.image_width, camera.image_height, this->scene.name.c_str()),
      sceneEntries(sceneRegistry()), renderer(camera.image_width, camera.image_height),
      timeSeed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())), runSeed(timeSeed) {
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
    renderer.addRenderPass(std::make_unique<BloomPass>(camera.image_width, camera.image_height));
    renderer.addRenderPass(std::make_unique<AutoExposurePass>(settings.exposure));
    renderer.addRenderPass(std::make_unique<TonemapPass>());
    renderer.addRenderPass(std::make_unique<TaaPass>());
    renderer.addRenderPass(std::make_unique<AovPass>());

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
            resetAccumulation();
            historyFrames = 0;
        }

        const InputState input = window.pollInput();
        camera.update(input, fpsTimer.deltaTime);

        if (camera.moving) {
            resetAccumulation();
            camera.moving = false;
        }

        RenderContext ctx{
            .scene = scene,
            .camera = camera,
            .settings = settings,
            .frameIndex = ++frameIndex,
            .timeSeed = timeSeed++,
            .dt = static_cast<float>(fpsTimer.deltaTime),
            .runSeed = samplerSeed(),
            .historyFrames = ++historyFrames,
        };

        // Keyed on the never-reset counter: frameIndex sits at 1 for as long as the camera
        // moves, which would pin the jitter and leave TAA nothing new to resolve.
        camera.applyJitter(static_cast<int>(framesRendered++));
        renderer.updateCameraUbo(camera);
        if (renderer.reloadShadersIfChanged()) {
            resetAccumulation();
            ctx.frameIndex = 0;
            historyFrames = 0;
            ctx.historyFrames = 0;
        }

        gpuTimer.start();
        renderer.render(ctx);
        gpuTimer.end();

        Gui::drawStats(fpsTimer, gpuTimer, renderer.getPassTimings(), scene, camera, sceneEntries, sceneSwitch, settings);

        window.getFrameBufferSize();
        if (input.debugGBufferNormal) {
            renderer.blitGBufferAttachmentToSwapChain(GBuffer::ATTACH_NORMAL, window.width, window.height);
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
    sceneSwitch.requested = -1;
    Log::info("Switching scene to '{}'", sceneEntries[idx].name);

    // Build and upload the new scene before replacing anything, so a missing asset leaves the
    // current scene running instead of half-replaced.
    try {
        Scene  next = sceneEntries[idx].factory();
        Camera nextCamera(next.cameraSettings);
        nextCamera.resize(window.width, window.height);
        renderer.loadScene(next, nextCamera);
        scene = std::move(next);
        camera = std::move(nextCamera);
    } catch (const std::exception& e) {
        Log::error("Scene '{}' failed to load, keeping '{}': {}", sceneEntries[idx].name, scene.name, e.what());
        return;
    }

    window.setTitle(scene.name);
    resetAccumulation();
    historyFrames = 0;

    sceneSwitch.current = idx;
}

void Application::resetAccumulation() {
    frameIndex = 0;
    ++accumulationEpoch;
}

uint32_t Application::samplerSeed() const {
    return runSeed + accumulationEpoch * 0x9e3779b9u;
}

Application::~Application() {
    Gui::shutdown();
}
