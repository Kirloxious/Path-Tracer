#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <ostream>
#include <vector>

#include "renderer.h"
#include "window.h"
#include "compute_shader.h"
#include "world.h"
#include "camera.h"
#include "buffer.h"

#define MAX_NUM_SPHERES 10

static ComputeShader compute;
static const std::filesystem::path computeShaderPath = "shader/compute_shader.comp";

static void ErrorCallback(int error, const char* description)
{
	std::cerr << "Error "<< error << ": " << description << std::endl;
}


int main() {

    glfwSetErrorCallback(ErrorCallback);
    
    CameraSettings camSettings{};
    camSettings.aspect_ratio = 16.0f / 9.0f;
    camSettings.image_width = 1200;

    camSettings.max_bounces = 16;
    
    camSettings.vfov = 20.0;

    camSettings.lookfrom = glm::vec3(13, 2, 3);
    camSettings.lookat = glm::vec3(0, 0, 0);

    Camera camera = Camera(camSettings);
    
    Window window(camera.image_width, camera.image_height, "window");
    
    window.makeCurrentContext();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glfwSwapInterval(0); // disable vsync

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Image Dimensions: " << camera.image_width << " x " << camera.image_height << std::endl;

    World world;
    World::buildSphereWorld(world);


    Buffer spheres_ssbo(GL_SHADER_STORAGE_BUFFER, 0, world.spheres, GL_DYNAMIC_READ);    
    Buffer mats_ssbo(GL_SHADER_STORAGE_BUFFER, 1, world.materials, GL_DYNAMIC_READ);
    Buffer cam_ubo(GL_UNIFORM_BUFFER, 2, camera.data, GL_STATIC_READ);
    Buffer bvhnodes_ssbo(GL_SHADER_STORAGE_BUFFER, 3, world.bvh, GL_DYNAMIC_READ);
    

    unsigned int num_objects = world.spheres.size();
    compute = ComputeShader(computeShaderPath);
    compute.use();
    compute.setInt("num_objects", num_objects);
    compute.setVec2("imageDimensions", glm::vec2(camera.image_width, camera.image_height));
    compute.setInt("bvh_size", world.bvhSize);
    compute.setInt("root_index", world.bvhRoot);
    compute.setInt("samples_per_pixel", camera.settings.samples_per_pixel);
    compute.setInt("max_bounces", camera.settings.max_bounces);
    compute.setInt("emissive_last_index", world.emissiveLastIndex);
    compute.setInt("num_spheres",    (int)world.spheres.size());
    



    Texture texture = createTexture(window.m_Width, window.m_Height);

    FrameBuffer fb = createFrameBuffer(texture);

    GLuint queryID;
    glGenQueries(1, &queryID);

    int frameIndex = 0; // used for accumulating image
    int frameCount = 0; // fps counting
    double deltaTime = 0;
    double lastTime = glfwGetTime();
    double timer = lastTime;

    const GLuint workGroupSizeX = 16;
    const GLuint workGroupSizeY = 16;
    
    GLuint numGroupsX = (camera.image_width + workGroupSizeX - 1) / workGroupSizeX;
    GLuint numGroupsY = (camera.image_height + workGroupSizeY - 1) / workGroupSizeY;

    while(!window.shouldClose()){

        if (camera.moving) {
            frameIndex = 0; // resets the frame accumulation in compute if camera moved last frame
            camera.moving = false;
        }

        camera.update(window.m_Window, deltaTime, camera);
        camera.updateInvMatrices();
        cam_ubo.update(camera.data);

        // Compute 
        {
            ++frameIndex;
            compute.use();
            compute.setInt("time", clock());
            compute.setInt("frameIndex", frameIndex);
            
            bindDoubleBufferTexture(texture);
            
            glBeginQuery(GL_TIME_ELAPSED, queryID); // Computer shader timer start
			glDispatchCompute(numGroupsX, numGroupsY, 1);
            glEndQuery(GL_TIME_ELAPSED);            // Computer shader timer end
            
            // make sure writing to image has finished before read
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            
        }

        GLuint64 executionTime;
        glGetQueryObjectui64v(queryID, GL_QUERY_RESULT, &executionTime); // computer shader timer result

        blitFrameBuffer(fb);
        
        window.swapBuffers();
        window.pollEvents();
        
        double currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        ++frameCount;

        
        if (currentTime - timer >= 1.0) {
            std::cout << "FPS: " << frameCount << " | Frame Time: " << (1000.0 / float(frameCount)) << " ms" << " | Compute Time: " << (executionTime / 1e6) << " ms" << std::endl;
            frameCount = 0;
            timer = currentTime;
        }
    }


    return 0;
}
