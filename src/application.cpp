#include "application.h"

#include <cmath>
#include <glm/glm.hpp>

#include "log.h"

static const std::filesystem::path computeShaderPath = "shader/compute_shader.comp";
static const std::filesystem::path denoiserShaderPath = "shader/denoiser.comp";

Application::Application(Scene scene)
    : scene(std::move(scene)), camera(this->scene.cameraSettings), window(camera.image_width, camera.image_height, this->scene.name.c_str()) {

    window.makeCurrentContext();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Log::error("Failed to initialize GLAD");
        return;
    }
    glfwSwapInterval(0);

    Log::info("OpenGL version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    Log::info("Image dimensions: {} x {}", camera.image_width, camera.image_height);

    compute = ComputeShader(computeShaderPath);
    denoiser = ComputeShader(denoiserShaderPath);

    accum = Texture(camera.image_width, camera.image_height);
    normals_tex = Texture(camera.image_width, camera.image_height, GL_RGBA16F);
    denoised_ping = Texture(camera.image_width, camera.image_height);
    display = Texture(camera.image_width, camera.image_height);
    fb = FrameBuffer(display);

    constexpr GLuint workGroupSize = 8;
    numGroupsX = (camera.image_width + workGroupSize - 1) / workGroupSize;
    numGroupsY = (camera.image_height + workGroupSize - 1) / workGroupSize;

    glGenQueries(2, queryIDs);
    for (int i = 0; i < 2; ++i) {
        glBeginQuery(GL_TIME_ELAPSED, queryIDs[i]);
        glEndQuery(GL_TIME_ELAPSED);
    }

    loadScene(std::move(this->scene));

    Log::info("Image dimensions after load scence: {} x {}", camera.image_width, camera.image_height);
    lastTime = glfwGetTime();
    timer = lastTime;
}

void Application::loadScene(Scene newScene) {
    scene = std::move(newScene);
    camera = Camera(scene.cameraSettings);

    World& world = scene.world;

    spheres_ssbo = Buffer(GL_SHADER_STORAGE_BUFFER, 0, world.spheres, GL_STATIC_DRAW);
    mats_ssbo = Buffer(GL_SHADER_STORAGE_BUFFER, 1, world.materials, GL_STATIC_DRAW);
    cam_ubo = Buffer(GL_UNIFORM_BUFFER, 2, camera.data, GL_DYNAMIC_DRAW);
    bvhnodes_ssbo = Buffer(GL_SHADER_STORAGE_BUFFER, 3, world.bvh.nodes, GL_STATIC_DRAW);
    triangles_ssbo = Buffer(GL_SHADER_STORAGE_BUFFER, 4, world.triangles, GL_STATIC_DRAW);

    uploadStaticUniforms();
    uploadDenoiserUniforms();

    frameIndex = 0;
}

Application::~Application() {
    if (queryIDs[0]) {
        glDeleteQueries(2, queryIDs);
    }
}

void Application::uploadStaticUniforms() {
    World& world = scene.world;

    compute.use();
    compute.setInt("num_objects", static_cast<int>(world.spheres.size()));
    compute.setVec2("image_dimensions", glm::vec2(camera.image_width, camera.image_height));
    compute.setInt("bvh_size", static_cast<int>(world.bvh.nodes.size()));
    compute.setInt("root_index", world.bvh.root);
    compute.setInt("samples_per_pixel", camera.settings.samples_per_pixel);
    compute.setInt("max_bounces", camera.settings.max_bounces);
    compute.setInt("emissive_last_index", world.emissiveLastIndex);
    compute.setInt("num_spheres", static_cast<int>(world.spheres.size()));
    compute.setInt("num_triangles", static_cast<int>(world.triangles.size()));
}

void Application::uploadDenoiserUniforms() {
    denoiser.use();
    denoiser.setVec2("image_size", glm::vec2(camera.image_width, camera.image_height));
    denoiser.setFloat("sigma_normal", 64.0f);
}

int Application::run() {
    // Im gui init
    // IMGUI_CHECKVERSION();
    // ImGui::CreateContext();
    //
    // // Setup Platform/Renderer backends
    // ImGui_ImplGlfw_InitForOpenGL(window.window, true);          // Second param install_callback=true will install GLFW callbacks and chain to
    // existing ones. ImGui_ImplOpenGL3_Init();

    while (!window.shouldClose()) {

        camera.update(window.pollInput(), deltaTime);

        if (camera.moving) {
            cam_ubo.update(camera.data);
            frameIndex = 0;
            camera.moving = false;
        }

        if (compute.reloadIfChanged()) {
            uploadStaticUniforms();
            frameIndex = 0;
        }
        if (denoiser.reloadIfChanged()) {
            uploadDenoiserUniforms();
        }

        // Path trace + denoise
        {
            ++frameIndex;
            compute.use();
            compute.setInt("time", static_cast<int>(timeSeed++));
            compute.setInt("frame_index", frameIndex);

            accum.bindForAccumulation();
            normals_tex.bind(2, GL_WRITE_ONLY);

            glBeginQuery(GL_TIME_ELAPSED, queryIDs[queryFrame]);
            glDispatchCompute(numGroupsX, numGroupsY, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            // A-Trous denoiser: 4 ping-pong passes, result ends in display
            Texture* srcs[4] = {&accum, &denoised_ping, &display, &denoised_ping};
            Texture* dsts[4] = {&denoised_ping, &display, &denoised_ping, &display};
            int      steps[4] = {1, 2, 4, 8};

            const float adaptiveSigmaColor = 1.0f / std::sqrt(static_cast<float>(frameIndex));

            denoiser.use();
            denoiser.setFloat("sigma_color", adaptiveSigmaColor);
            for (int pass = 0; pass < 4; ++pass) {
                srcs[pass]->bind(0, GL_READ_ONLY);
                normals_tex.bind(1, GL_READ_ONLY);
                dsts[pass]->bind(2, GL_WRITE_ONLY);
                denoiser.setInt("step_size", steps[pass]);
                denoiser.setInt("apply_tonemapping", pass == 3 ? 1 : 0);
                glDispatchCompute(numGroupsX, numGroupsY, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }

            glEndQuery(GL_TIME_ELAPSED);
        }

        // Read the *previous* frame's query result (always available, no CPU stall)
        int prevQuery = 1 - queryFrame;
        glGetQueryObjectui64v(queryIDs[prevQuery], GL_QUERY_RESULT, &lastComputeTime);
        queryFrame = prevQuery;

        window.getFrameBufferSize();
        fb.blit(display, window.width, window.height);
        //
        // ImGui_ImplOpenGL3_NewFrame();
        // ImGui_ImplGlfw_NewFrame();
        // ImGui::NewFrame();
        // ImGui::ShowDemoWindow(); // Show demo window! :)
        //
        // ImGui::Render();
        // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        //
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
    //
    // ImGui_ImplOpenGL3_Shutdown();
    // ImGui_ImplGlfw_Shutdown();
    // ImGui::DestroyContext();

    return 0;
}
