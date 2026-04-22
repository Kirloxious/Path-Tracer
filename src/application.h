#pragma once

#include <cstdint>

#include "renderer.h"
#include "window.h"
#include "camera.h"
#include "scene.h"
#include "buffer.h"
#include "timer.h"

#include "gui.h"

class Application
{
public:
    explicit Application(Scene scene);
    ~Application();

    void loadScene(Scene scene);
    int  run();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

private:
    // void uploadStaticUniforms();
    // void uploadDenoiserUniforms();

    Scene    scene;
    Camera   camera;
    Window   window;
    Renderer renderer;

    GPUTimer gpuTimer;
    FPSTimer fpsTimer;

    uint32_t timeSeed = 0;
    int      frameIndex = 0;
};
