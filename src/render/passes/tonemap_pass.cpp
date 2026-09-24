#include "render/passes/tonemap_pass.h"

#include "core/log.h"
#include "gpu/gl.h"

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
    targets.tonemapped.bind(1, GL_WRITE_ONLY);
    GL::dispatch(targets.numGroupsX, targets.numGroupsY);
    GL::memoryBarrier(GL::Barrier::ImageAccess);
}
