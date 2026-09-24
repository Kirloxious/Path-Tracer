#include "render/renderer.h"
#include "gpu/buffer.h"
#include "core/log.h"
#include "render/render_pass.h"
#include "render/render_targets.h"
#include <memory>

Renderer::Renderer(int w, int h) : targets(w, h) {
    Log::info("Renderer");
}

void Renderer::loadScene(const Scene& scene, const Camera& camera) {
    // An unlit scene still gets one zeroed group, so binding 0 never keeps the previous
    // scene's lights; the shaders gate every read on num_light_groups.
    const std::vector<World::LightGroup> noLights(1);
    const auto&                          lightGroups = scene.world.lightGroups.empty() ? noLights : scene.world.lightGroups;
    lightGroupsSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 0, lightGroups, GL_STATIC_DRAW);
    matsSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 1, scene.world.materials, GL_STATIC_DRAW);
    camUBO = Buffer(GL_UNIFORM_BUFFER, 2, camera.data, GL_DYNAMIC_DRAW); // updated every frame
    bvhNodesSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 3, scene.world.bvh.nodes, GL_STATIC_DRAW);
    trianglesSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 4, scene.world.triangles, GL_STATIC_DRAW);
    verticesSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 5, scene.world.vertices, GL_STATIC_DRAW);
    // Leaf triangle references. A BVH leaf owns a contiguous run here; each entry indexes
    // trianglesSSBO. The indirection is what lets leaves batch several triangles without
    // disturbing the emissive-first triangle ordering that NEE and the shadow ray rely on.
    triRefsSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 26, scene.world.bvh.triRefs, GL_STATIC_DRAW);

    // Rebuild (or clear) the envmap. Empty path → EnvMap() default-constructs
    // to invalid, which makes `targets.envMap->valid()` false.
    if (!scene.envMapPath.empty()) {
        envMap = EnvMap(scene.envMapPath, scene.envIntensity);
    } else {
        envMap = EnvMap();
    }
    targets.envMap = &envMap;

    // The importance-sampling table. Always uploaded, even for a scene with no envmap: the
    // shaders gate every read on env_map_valid, but leaving the binding empty would make a
    // stray read undefined rather than merely wrong, and one cell costs nothing.
    const std::vector<EnvSampleCell> fallback(1);
    envSamplingSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, 27, envMap.samplingCells().empty() ? fallback : envMap.samplingCells(), GL_STATIC_DRAW);

    Log::info("Renderer: Buffers created");
    for (auto& pass : passes) {
        pass->uploadUniforms(scene, camera);
    }

    Log::info("Renderer: Scene loaded.");
}

void Renderer::resize(int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    targets.resize(w, h);
    for (auto& pass : passes) {
        pass->resize(w, h);
    }
}

void Renderer::updateCameraUbo(const Camera& cam) {
    camUBO.update(cam.data);
}

void Renderer::render(RenderContext& ctx) {
    // Pull in previous frame's per-pass timestamps before we overwrite them.
    passTimings.beginFrame();
    for (size_t i = 0; i < passes.size(); ++i) {
        passTimings.beginPass(static_cast<int>(i));
        passes[i]->execute(ctx, targets);
        passTimings.endPass(static_cast<int>(i));
    }
    passTimings.endFrame();
}

bool Renderer::reloadShadersIfChanged(RenderContext& ctx) {
    bool changed = false;
    for (auto& pass : passes) {
        changed |= pass->reloadIfChanged(ctx);
    }

    return changed;
}
void Renderer::addRenderPass(std::unique_ptr<RenderPass> pass) {
    passTimings.addPass(pass->name());
    passes.push_back(std::move(pass));
}

void Renderer::blitToSwapChain(int width, int height) {
    targets.fb.blit(width, height);
}

void Renderer::blitGBufferAttachmentToSwapChain(int attachmentIndex, int width, int height) {
    targets.gbuf.blitAttachmentToSwapChain(attachmentIndex, width, height);
}
