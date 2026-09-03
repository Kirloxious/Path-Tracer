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
 * TAA → AOV → Gui — and calls Renderer::loadScene(). GuiPass must stay last so the overlay
 * draws over the resolved image.
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
    /// Rebuilds the GPU buffers and re-fires uploadUniforms() when the GUI has requested a
    /// different scene. No-op when `sceneSwitch.requested` is -1.
    void applyPendingSceneSwitch();

    static Scene defaultScene() { return Scene::CornellBox(); };

    Scene    scene;
    Camera   camera;
    Window   window;
    Renderer renderer;

    GPUTimer gpuTimer;
    FPSTimer fpsTimer;

    std::vector<SceneEntry> sceneEntries;
    Gui::SceneSwitchState   sceneSwitch;
    RenderSettings          settings;

    /// Seeded from system time at construction so different runs don't share frame-1 noise.
    uint32_t timeSeed;
    int      frameIndex = 0;
};
