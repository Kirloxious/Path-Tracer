#include "render/passes/aov_pass.h"

#include <glad/glad.h>

#include "core/log.h"
#include "core/shader_shared.h"

AovPass::AovPass(const RenderSettings& s) : settings(s) {
    Log::info("AovPass: loading 'shader/aov.comp'");
    shader = ComputeShader("shader/aov.comp");
}

bool AovPass::reloadIfChanged() {
    return shader.reloadIfChanged();
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
    glBindTextureUnit(TEX_GBUF_NORMAL, targets.gbuf.normal.handle);
    glBindTextureUnit(TEX_GBUF_DEPTH, targets.gbuf.depth.handle);

    shader.setInt("aov_mode", static_cast<int>(settings.aovMode));
    shader.setFloat("depth_max", settings.aovDepthMax);
    shader.setFloat("bvh_cost_max", settings.aovBvhCostMax);

    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
