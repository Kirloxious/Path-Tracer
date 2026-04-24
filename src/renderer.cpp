#include "renderer.h"
#include "buffer.h"
#include "log.h"
#include "render_pass.h"
#include "render_targets.h"
#include <memory>

Renderer::Renderer(int w, int h) : targets(w, h) {
    Log::info("Renderer");
    passes.reserve(5);
}

void Renderer::loadScene(const Scene& scene, const Camera& camera) {
    spheresSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 0, scene.world.spheres, GL_STREAM_COPY);
    matsSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 1, scene.world.materials, GL_STREAM_COPY);
    camUBO = Buffer(GL_UNIFORM_BUFFER, 2, camera.data, GL_DYNAMIC_DRAW);
    bvhNodesSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 3, scene.world.bvh.nodes, GL_STREAM_COPY);
    trianglesSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 4, scene.world.triangles, GL_STREAM_COPY);

    Log::info("Renderer: Buffers created");
    for (auto& pass : passes) {
        // hack: passing temp render ctx
        pass->uploadUniforms({scene, camera, 1, 0});
    }

    Log::info("Renderer: Scene loaded.");
}

void resize(int w, int h);

void Renderer::updateCameraUbo(const Camera& cam) {
    camUBO.update(cam.data);
}

Texture& Renderer::render(RenderContext& ctx) {
    for (auto& pass : passes) {
        pass->execute(ctx, targets);
    }

    return targets.display;
}

bool Renderer::reloadShadersIfChanged(RenderContext& ctx) {
    bool changed = false;
    for (auto& pass : passes) {
        changed |= pass->reloadIfChanged(ctx);
    }

    return changed;
}
void Renderer::addRenderPass(std::unique_ptr<RenderPass> pass) {
    passes.push_back(std::move(pass));
}

void Renderer::blitToSwapChain(Texture& renderOutput, int width, int height) {
    targets.fb.blit(renderOutput, width, height);
}

void Renderer::blitGBufferAttachmentToSwapChain(int attachmentIndex, int width, int height) {
    targets.gbuf.blitAttachmentToSwapChain(attachmentIndex, width, height);
}
