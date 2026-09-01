#include "render/passes/taa_pass.h"

#include <utility>

#include <glad/glad.h>

#include "core/log.h"

TaaPass::TaaPass(int w, int h) : width(w), height(h) {
    Log::info("TaaPass: loading 'shader/taa.comp'");
    shader = ComputeShader("shader/taa.comp");
}

bool TaaPass::reloadIfChanged(const RenderContext&) {
    return shader.reloadIfChanged();
}

void TaaPass::resize(int w, int h) {
    width = w;
    height = h;
}

void TaaPass::execute(const RenderContext& ctx, RenderTargets& targets) {
    shader.use();

    targets.display.bind(0, GL_READ_ONLY);
    targets.taa_output.bind(2, GL_WRITE_ONLY);
    glBindTextureUnit(6, targets.gbuf.normal.handle);
    glBindTextureUnit(10, targets.gbuf.depth.handle); // world position is reconstructed from this
    glBindTextureUnit(7, targets.taa_history.handle);

    shader.setIVec2("image_size", width, height);
    shader.setInt("frame_index", ctx.frameIndex);
    // High history weight so per-frame jitter mostly cancels out. Catmull-Rom
    // resampling keeps this from turning into visible blur (which pure bilinear at
    // this weight would).
    shader.setFloat("blend_alpha", 0.90f);

    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Copy the TAA result back into display so downstream passes (AOV overrides,
    // swap-chain blit) read the resolved image without any renaming.
    glCopyImageSubData(targets.taa_output.handle, GL_TEXTURE_2D, 0, 0, 0, 0, targets.display.handle, GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);

    // Rotate history: this frame's output becomes next frame's history. The old
    // history moves into taa_output where it'll be overwritten next frame. Both
    // textures were created with LINEAR filtering (see render_targets.cpp) so the
    // swap doesn't disturb bilinear reprojection.
    std::swap(targets.taa_output, targets.taa_history);
}
