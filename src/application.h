#pragma once

#include <cstdint>

#include "window.h"
#include "camera.h"
#include "scene.h"
#include "buffer.h"
#include "compute_shader.h"
#include "renderer.h"

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
    void uploadStaticUniforms();
    void uploadDenoiserUniforms();

    Scene  scene;
    Camera camera;
    Window window;

    Buffer spheres_ssbo;
    Buffer mats_ssbo;
    Buffer cam_ubo;
    Buffer bvhnodes_ssbo;
    Buffer triangles_ssbo;

    ComputeShader compute;
    ComputeShader denoiser;

    Texture     accum;
    Texture     normals_tex;
    Texture     denoised_ping;
    Texture     display;
    FrameBuffer fb;

    GLuint numGroupsX = 0;
    GLuint numGroupsY = 0;

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
