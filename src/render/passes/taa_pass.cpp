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

    targets.display.bind(0, GL_READ_ONLY);
    targets.taa_output.bind(2, GL_WRITE_ONLY);
    targets.taa_history.bindSampler(TEX_TAA_HISTORY);

    // High history weight so per-frame jitter mostly cancels out. Catmull-Rom
    // resampling keeps this from turning into visible blur (which pure bilinear at
    // this weight would).
    shader.setFloat("blend_alpha", 0.90f);

    GL::dispatch(targets.numGroupsX, targets.numGroupsY);
    GL::memoryBarrier(GL::Barrier::ImageAccess);

    // Copy the TAA result back into display so downstream passes (AOV overrides,
    // swap-chain blit) read the resolved image without any renaming.
    targets.taa_output.copyTo(targets.display);
}
