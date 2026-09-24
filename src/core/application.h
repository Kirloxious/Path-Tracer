#pragma once

/**
 * @file application.h
 * @brief Top-level owner of the window, scene, camera, renderer and main loop.
 */

#include <cstdint>
#include <vector>

#include "render/renderer.h"
#include "core/window.h"
#include "scene/camera.h"
#include "render/render_settings.h"
#include "scene/scene.h"
#include "gpu/timer.h"

#include "render/gui.h"

/**
 * @brief Owns every long-lived subsystem and drives the frame loop.
 *
 * The constructor creates the GL context (via Window), registers the render passes in their
 * fixed order — Raster → ReSTIR → PathTracer → Denoiser → Bloom → AutoExposure → Tonemap →
 * TAA → AOV — and calls Renderer::loadScene(). The ImGui overlay is built after the passes
 * and drawn after the swap-chain blit, so it always sits on top of the image.
 *
 * Non-copyable: it owns GL objects and a GLFW window.
 */
class Application
{
public:
    /**
     * @brief Builds the window, renderer and pass list, then uploads @p scene to the GPU.
     *
     * @param scene The initial scene, moved into the Application. Its `name` becomes the
     *              window title and its CameraSettings seed the Camera.
     */
    explicit Application(Scene scene = Application::defaultScene());
    ~Application();

    /**
     * @brief Runs the main loop until the window is asked to close.
     *
     * Each iteration polls input, drains any pending framebuffer resize, updates the camera,
     * advances `frameIndex`/`timeSeed`, runs every registered pass, and presents. Progressive
     * accumulation restarts (`frameIndex` back to 0) whenever the camera moves, a shader
     * hot-reload succeeds, the window is resized, or a scene switch is applied.
     *
     * @return 0 on a normal exit.
     */
    int run();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

private:
    /// Loads the scene the GUI requested, if any. A scene that fails to build or upload is
    /// logged and the current one keeps running.
    void applyPendingSceneSwitch();
    /// Restarts progressive accumulation and moves the sampler onto a fresh scramble.
    void resetAccumulation();
    /// Per-pixel sampler seed for the current accumulation; see RenderContext::runSeed.
    [[nodiscard]] uint32_t samplerSeed() const;

    static Scene defaultScene() { return Scene::CornellBox(); };

    Scene  scene;
    Camera camera;
    Window window;

    // Needs the GL context, so it must be constructed after `window`.
    GPUTimer gpuTimer;
    FPSTimer fpsTimer;

    std::vector<SceneEntry> sceneEntries;
    Gui::SceneSwitchState   sceneSwitch;
    RenderSettings          settings;

    Renderer renderer;

    /// Seeded from system time at construction so different runs don't share frame-1 noise.
    uint32_t timeSeed;
    /// Fixed at construction; combined with accumulationEpoch into RenderContext::runSeed.
    uint32_t runSeed;
    int      frameIndex = 0;
    /// Bumped on every accumulation reset. A moving camera resets every frame, and without a
    /// new scramble each of those frames would redraw sample 1 of the same sequence.
    uint32_t accumulationEpoch = 0;
    /// Never reset. Drives the sub-pixel jitter.
    uint32_t framesRendered = 0;
    /// See RenderContext::historyFrames.
    int historyFrames = 0;
};
