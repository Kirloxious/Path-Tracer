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
    explicit Application(Scene scene);
    ~Application();

    int run();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

private:
    void applyPendingSceneSwitch();

    Scene    scene;
    Camera   camera;
    Window   window;
    Renderer renderer;

    GPUTimer gpuTimer;
    FPSTimer fpsTimer;

    std::vector<SceneEntry> sceneEntries;
    Gui::SceneSwitchState   sceneSwitch;
    RenderSettings          settings;

    // Seeded from system time at construction so different runs don't share frame-1 noise.
    uint32_t timeSeed;
    int      frameIndex = 0;
};
