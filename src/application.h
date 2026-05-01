#pragma once

#include <cstdint>
#include <vector>

#include "renderer.h"
#include "window.h"
#include "camera.h"
#include "scene.h"
#include "timer.h"

#include "gui.h"

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

    // Seeded from system time at construction so different runs don't share frame-1 noise.
    uint32_t timeSeed;
    int      frameIndex = 0;
};
