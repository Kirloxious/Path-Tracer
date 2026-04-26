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

    uint32_t timeSeed = 0;
    int      frameIndex = 0;
};
