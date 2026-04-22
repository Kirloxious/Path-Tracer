#include "application.h"

#include <memory>

#include "log.h"
#include "render_pass.h"
#include "renderer.h"
#include "path_tracer_pass.h"
#include "denoiser_pass.h"
#include "texture.h"

static const std::filesystem::path computeShaderPath = "shader/compute_shader.comp";
static const std::filesystem::path denoiserShaderPath = "shader/denoiser.comp";

Application::Application(Scene scene)
    : scene(std::move(scene)), camera(this->scene.cameraSettings), window(camera.image_width, camera.image_height, this->scene.name.c_str()),
      renderer(camera.image_width, camera.image_height) {

    Log::info("OpenGL version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    Log::info("Image dimensions: {} x {}", camera.image_width, camera.image_height);

    Log::info("Adding render passes");
    renderer.addRenderPass(std::make_unique<PathTracerPass>(computeShaderPath));
    renderer.addRenderPass(std::make_unique<DenoiserPass>(denoiserShaderPath));

    loadScene(std::move(this->scene));

    // Move into timer class
    lastTime = glfwGetTime();
    timer = lastTime;

    glGenQueries(2, queryIDs);
    for (int i = 0; i < 2; ++i) {
        glBeginQuery(GL_TIME_ELAPSED, queryIDs[i]);
        glEndQuery(GL_TIME_ELAPSED);
    }
}

void Application::loadScene(Scene newScene) {
    scene = std::move(newScene);
    camera = Camera(scene.cameraSettings);
    renderer.loadScene(scene, camera);
    frameIndex = 0;
}

Application::~Application() {
    if (queryIDs[0]) {
        glDeleteQueries(2, queryIDs);
    }
}

// void Application::uploadStaticUniforms() {
//     World& world = scene.world;
//
//     compute.use();
//     compute.setInt("num_objects", static_cast<int>(world.spheres.size()));
//     compute.setVec2("image_dimensions", glm::vec2(camera.image_width, camera.image_height));
//     compute.setInt("bvh_size", static_cast<int>(world.bvh.nodes.size()));
//     compute.setInt("root_index", world.bvh.root);
//     compute.setInt("samples_per_pixel", camera.settings.samples_per_pixel);
//     compute.setInt("max_bounces", camera.settings.max_bounces);
//     compute.setInt("emissive_last_index", world.emissiveLastIndex);
//     compute.setInt("num_spheres", static_cast<int>(world.spheres.size()));
//     compute.setInt("num_triangles", static_cast<int>(world.triangles.size()));
// }
//
// void Application::uploadDenoiserUniforms() {
//     denoiser.use();
//     denoiser.setVec2("image_size", glm::vec2(camera.image_width, camera.image_height));
//     denoiser.setFloat("sigma_normal", 64.0f);
// }

int Application::run() {

    while (!window.shouldClose()) {

        camera.update(window.pollInput(), deltaTime);

        if (camera.moving) {
            renderer.updateCameraUbo(camera);
            frameIndex = 0;
            camera.moving = false;
        }

        RenderContext ctx{scene, camera, ++frameIndex, timeSeed++};
        if (renderer.reloadShadersIfChanged(ctx)) {
            frameIndex = 0;
            ctx.frameIndex = 0;
        }

        glBeginQuery(GL_TIME_ELAPSED, queryIDs[queryFrame]);
        Texture& output = renderer.render(ctx);
        glEndQuery(GL_TIME_ELAPSED);

        int prevQuery = 1 - queryFrame;
        glGetQueryObjectui64v(queryIDs[prevQuery], GL_QUERY_RESULT, &lastComputeTime);
        queryFrame = prevQuery;

        window.getFrameBufferSize();
        renderer.blitToSwapChain(output, window.width, window.height);

        window.pollEvents();
        window.swapBuffers();

        double currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        ++frameCount;

        if (currentTime - timer >= 1.0) {
            Log::info("FPS: {} | Frame: {:.2f} ms | Compute: {:.2f} ms", frameCount, 1000.0 / frameCount, lastComputeTime / 1e6);
            frameCount = 0;
            timer = currentTime;
            Log::info("Image size: {} x {}", window.width, window.height);
        }
    }

    return 0;
}
