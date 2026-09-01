#include "render/passes/aov_pass.h"

#include <glad/glad.h>

#include "core/log.h"
#include "scene/scene.h"
#include "scene/world.h"

AovPass::AovPass(int w, int h, const RenderSettings& s) : width(w), height(h), settings(s) {
    Log::info("AovPass: loading 'shader/aov.comp'");
    shader = ComputeShader("shader/aov.comp");
}

void AovPass::uploadUniforms(const Scene& scene, const Camera&) {
    shader.use();
    shader.setInt("bvh_root_index", scene.world.bvh.root);
}

void AovPass::resize(int w, int h) {
    width = w;
    height = h;
}

bool AovPass::reloadIfChanged(const RenderContext& ctx) {
    if (shader.reloadIfChanged()) {
        uploadUniforms(ctx.scene, ctx.camera);
        return true;
    }
    return false;
}

void AovPass::execute(const RenderContext&, RenderTargets& targets) {
    if (settings.aovMode == AovMode::None) {
        return;
    }

    shader.use();
    // display is bound as image so we overwrite the tonemapped denoiser output.
    // accum is bound read-only for the variance AOV.
    targets.display.bind(0, GL_WRITE_ONLY);
    targets.accum.bind(1, GL_READ_ONLY);
    glBindTextureUnit(6, targets.gbuf.normal.handle);
    glBindTextureUnit(10, targets.gbuf.depth.handle); // world position is reconstructed from this

    shader.setIVec2("image_size", width, height);
    shader.setInt("aov_mode", static_cast<int>(settings.aovMode));
    shader.setFloat("depth_max", settings.aovDepthMax);
    shader.setFloat("bvh_cost_max", settings.aovBvhCostMax);

    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
