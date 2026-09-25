#include "render/passes/taa_pass.h"

#include "core/log.h"
#include "core/shader_shared.h"
#include "gpu/gl.h"

TaaPass::TaaPass() {
    Log::info("TaaPass: loading 'shader/taa.comp'");
    shader = ComputeShader("shader/taa.comp");
}

bool TaaPass::reloadIfChanged() {
    return shader.reloadIfChanged();
}

void TaaPass::execute(const RenderContext&, RenderTargets& targets) {
    shader.use();

    targets.tonemapped.bind(0, GL_READ_ONLY);
    targets.display.bind(1, GL_WRITE_ONLY);
    targets.taa_output.bind(2, GL_WRITE_ONLY);
    targets.taa_history.bindSampler(TEX_TAA_HISTORY);

    // High history weight so jitter cancels; Catmull-Rom resampling keeps it from blurring.
    shader.setFloat("blend_alpha", 0.90f);

    GL::dispatch(targets.numGroupsX, targets.numGroupsY);
    // `display` is next read by the swap-chain blit, and `taa_output` by next frame's sampler.
    GL::memoryBarrier(GL::Barrier::ImageAccess | GL::Barrier::Framebuffer | GL::Barrier::TextureFetch);
}
