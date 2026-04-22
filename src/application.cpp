#include "application.h"

#include <memory>

#include "gl_debug.h"
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

    GLDebug::enable();

    Log::info("Adding render passes");
    renderer.addRenderPass(std::make_unique<PathTracerPass>(computeShaderPath));
    renderer.addRenderPass(std::make_unique<DenoiserPass>(denoiserShaderPath));

    loadScene(std::move(this->scene));
}

void Application::loadScene(Scene newScene) {
    scene = std::move(newScene);
    camera = Camera(scene.cameraSettings);
    renderer.loadScene(scene, camera);
    frameIndex = 0;
}

Application::~Application() {}

int Application::run() {

    while (!window.shouldClose()) {

        camera.update(window.pollInput(), fpsTimer.deltaTime);

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

        gpuTimer.start();
        Texture& output = renderer.render(ctx);
        gpuTimer.end();

        window.getFrameBufferSize();
        renderer.blitToSwapChain(output, window.width, window.height);

        window.pollEvents();
        window.swapBuffers();

        fpsTimer.end();

        if (fpsTimer.currentTime - fpsTimer.timer >= 1.0) {
            Log::info("{} | {}", fpsTimer.formatted(), gpuTimer.formatted());
            fpsTimer.frameCountReset();
        }
    }

    return 0;
}
