#include "renderer.h"
#include "render_pass.h"
#include "render_targets.h"
#include <memory>

Renderer::Renderer(int w, int h) : targets(w, h), spheresSSBO(), matsSSBO(), camUBO(), bvhNodesSSBO(), trianglesSSBO() {
    passes.reserve(sizeof(std::unique_ptr<RenderPass>) * 5);
}

void Renderer::loadScene(RenderContext& ctx) {
    spheresSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 0, ctx.scene.world.spheres, GL_STATIC_DRAW);
    matsSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 1, ctx.scene.world.materials, GL_STATIC_DRAW);
    camUBO = Buffer(GL_UNIFORM_BUFFER, 2, ctx.camera.data, GL_DYNAMIC_DRAW);
    bvhNodesSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 3, ctx.scene.world.bvh.nodes, GL_STATIC_DRAW);
    trianglesSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 4, ctx.scene.world.triangles, GL_STATIC_DRAW);

    for (auto& pass : passes) {
        pass->uploadUniforms(ctx);
    }

    ctx.resetFrameIndex();
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
        changed = pass->reloadIfChanged(ctx);
    }

    return changed;
}
void Renderer::addRenderPass(std::unique_ptr<RenderPass> pass) {
    passes.push_back(std::move(pass));
}
