#include <glad/glad.h>
#include <glm/glm.hpp>
#include <filesystem>

#include "renderer.h"
#include "window.h"
#include "compute_shader.h"
#include "world.h"
#include "camera.h"
#include "buffer.h"
#include "log.h"

static const std::filesystem::path computeShaderPath = "shader/compute_shader.comp";

static void ErrorCallback(int error, const char* description) {
    Log::error("GLFW {}: {}", error, description);
}

int main() {

    glfwSetErrorCallback(ErrorCallback);

    CameraSettings camSettings{};
    camSettings.aspect_ratio = 16.0f / 9.0f;
    camSettings.image_width = 1200;

    camSettings.max_bounces = 16;

    camSettings.vfov = 20.0f;

    camSettings.lookfrom = glm::vec3(13, 2, 3);
    camSettings.lookat = glm::vec3(0, 0, 0);

    Camera camera(camSettings);

    Window window(camera.image_width, camera.image_height, "window");

    window.makeCurrentContext();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Log::error("Failed to initialize GLAD");
        return -1;
    }
    glfwSwapInterval(0); // disable vsync

    Log::info("OpenGL version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    Log::info("Image dimensions: {} x {}", camera.image_width, camera.image_height);

    World world = World::buildSphereWorld();

    Buffer spheres_ssbo(GL_SHADER_STORAGE_BUFFER, 0, world.spheres, GL_DYNAMIC_READ);
    Buffer mats_ssbo(GL_SHADER_STORAGE_BUFFER, 1, world.materials, GL_DYNAMIC_READ);
    Buffer cam_ubo(GL_UNIFORM_BUFFER, 2, camera.data, GL_STATIC_READ);
    Buffer bvhnodes_ssbo(GL_SHADER_STORAGE_BUFFER, 3, world.bvh.nodes, GL_DYNAMIC_READ);

    const auto    num_objects = static_cast<int>(world.spheres.size());
    ComputeShader compute(computeShaderPath);

    // Uploads all scene-static uniforms. Called once at startup and again after every hot reload.
    auto uploadStaticUniforms = [&]() {
        compute.use();
        compute.setInt("num_objects", num_objects);
        compute.setVec2("imageDimensions", glm::vec2(camera.image_width, camera.image_height));
        compute.setInt("bvh_size", static_cast<int>(world.bvh.nodes.size()));
        compute.setInt("root_index", world.bvh.root);
        compute.setInt("samples_per_pixel", camera.settings.samples_per_pixel);
        compute.setInt("max_bounces", camera.settings.max_bounces);
        compute.setInt("emissive_last_index", world.emissiveLastIndex);
        compute.setInt("num_spheres", static_cast<int>(world.spheres.size()));
    };
    uploadStaticUniforms();

    Texture     texture(window.m_Width, window.m_Height);
    FrameBuffer fb(texture);

    int      frameIndex = 0; // used for accumulating image
    int      frameCount = 0; // fps counting
    uint32_t timeSeed = 0;   // ever-increasing RNG seed (avoids clock() overflow)
    double   deltaTime = 0;
    double   lastTime = glfwGetTime();
    double   timer = lastTime;

    // 8x8 seems to be the best performances for the compute
    const GLuint workGroupSizeX = 8;
    const GLuint workGroupSizeY = 8;

    GLuint numGroupsX = (camera.image_width + workGroupSizeX - 1) / workGroupSizeX;
    GLuint numGroupsY = (camera.image_height + workGroupSizeY - 1) / workGroupSizeY;

    GLuint queryID;
    glGenQueries(1, &queryID);

    while (!window.shouldClose()) {

        if (camera.moving) {
            frameIndex = 0; // resets the frame accumulation in compute if camera moved last frame
            camera.moving = false;
        }

        camera.update(window.pollInput(), deltaTime);
        cam_ubo.update(camera.data);

        if (compute.reloadIfChanged()) {
            uploadStaticUniforms();
            frameIndex = 0;
        }

        // Compute
        {
            ++frameIndex;
            compute.use();
            compute.setInt("time", static_cast<int>(timeSeed++));
            compute.setInt("frameIndex", frameIndex);

            texture.bindAsDoubleBuffer();

            glBeginQuery(GL_TIME_ELAPSED, queryID); // Computer shader timer start
            glDispatchCompute(numGroupsX, numGroupsY, 1);
            glEndQuery(GL_TIME_ELAPSED); // Computer shader timer end

            // make sure writing to image has finished before read
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

        // Retrieve compute shader execution time from the GPU query
        GLuint64 executionTime;
        glGetQueryObjectui64v(queryID, GL_QUERY_RESULT, &executionTime);

        fb.blit(texture);

        window.swapBuffers();
        window.pollEvents();

        double currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        ++frameCount;

        if (currentTime - timer >= 1.0) {
            Log::info("FPS: {} | Frame: {:.2f} ms | Compute: {:.2f} ms", frameCount, 1000.0 / frameCount, executionTime / 1e6);
            frameCount = 0;
            timer = currentTime;
        }
    }

    return 0;
}
