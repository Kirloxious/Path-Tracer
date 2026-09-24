#include "render/passes/tonemap_pass.h"

#include "core/log.h"

TonemapPass::TonemapPass() {
    Log::info("TonemapPass: loading 'shader/tonemap.comp'");
    shader = ComputeShader("shader/tonemap.comp");
}

bool TonemapPass::reloadIfChanged() {
    return shader.reloadIfChanged();
}

void TonemapPass::execute(const RenderContext&, RenderTargets& targets) {
    shader.use();
    targets.hdr.bind(0, GL_READ_ONLY);
    targets.display.bind(1, GL_WRITE_ONLY);
    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
