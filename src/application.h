#pragma once

#include <cstdint>

#include "renderer.h"
#include "window.h"
#include "camera.h"
#include "scene.h"
#include "buffer.h"
#include "compute_shader.h"
#include "texture.h"
#include "frame_buffer.h"
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

    GLuint   queryIDs[2]{};
    int      queryFrame = 0;
    GLuint64 lastComputeTime = 0;

    int      frameIndex = 0;
    int      frameCount = 0;
    uint32_t timeSeed = 0;
    double   deltaTime = 0;
    double   lastTime = 0;
    double   timer = 0;
};
