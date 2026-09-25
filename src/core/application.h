#pragma once

#include <cstdint>
#include <vector>

#include "render/renderer.h"
#include "core/window.h"
#include "scene/camera.h"
#include "render/render_settings.h"
#include "scene/scene.h"
#include "gpu/timer.h"

#include "render/gui.h"

class Application
{
public:
    explicit Application(Scene scene = Application::defaultScene());
    ~Application();

    int run();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

private:
    /// A requested scene that fails to build or upload is logged; the current one keeps running.
    void applyPendingSceneSwitch();
    /// Also moves the sampler onto a fresh scramble.
    void                   resetAccumulation();
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

    uint32_t timeSeed;
    uint32_t runSeed;
    int      frameIndex = 0;
    /// A moving camera resets every frame; without a new scramble per reset each of those
    /// frames would redraw sample 1 of the same sequence.
    uint32_t accumulationEpoch = 0;
    /// Never reset; drives the sub-pixel jitter.
    uint32_t framesRendered = 0;
    int      historyFrames = 0;
};
