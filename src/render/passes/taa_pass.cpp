#include "render/passes/taa_pass.h"

#include <glad/glad.h>

#include "core/log.h"
#include "core/shader_shared.h"

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
    glBindTextureUnit(TEX_TAA_HISTORY, targets.taa_history.id());

    // High history weight so per-frame jitter mostly cancels out. Catmull-Rom
    // resampling keeps this from turning into visible blur (which pure bilinear at
    // this weight would).
    shader.setFloat("blend_alpha", 0.90f);

    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Copy the TAA result back into display so downstream passes (AOV overrides,
    // swap-chain blit) read the resolved image without any renaming.
    glCopyImageSubData(
        targets.taa_output.id(), GL_TEXTURE_2D, 0, 0, 0, 0, targets.display.id(), GL_TEXTURE_2D, 0, 0, 0, 0, targets.width, targets.height, 1);
}
