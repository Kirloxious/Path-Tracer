#include "denoiser_pass.h"

#include <algorithm>
#include <cmath>

#include "compute_shader.h"
#include "log.h"
#include "render_pass.h"

DenoiserPass::DenoiserPass(const std::filesystem::path& shaderPath) {
    Log::info("DenoiserPass: loading '{}'", shaderPath.string());
    shader = ComputeShader(shaderPath);
}

void DenoiserPass::uploadUniforms(const RenderContext& ctx) {
    shader.use();
    shader.setVec2("image_size", glm::vec2(ctx.camera.image_width, ctx.camera.image_height));
    shader.setFloat("sigma_normal", 64.0f);
}

bool DenoiserPass::reloadIfChanged(const RenderContext&) {
    return shader.reloadIfChanged();
}

void DenoiserPass::execute(const RenderContext& ctx, RenderTargets& targets) {

    // A-Trous denoiser: 4 ping-pong passes, result ends in display.
    Texture* srcs[4] = {&targets.accum, &targets.denoised_ping, &targets.display, &targets.denoised_ping};
    Texture* dsts[4] = {&targets.denoised_ping, &targets.display, &targets.denoised_ping, &targets.display};
    int      steps[4] = {1, 2, 4, 8};

    // Tighter than noise-stddev (1/sqrt(N)) so shadow penumbras — which live on
    // same-normal surfaces and rely solely on the color weight to stop the filter —
    // sharpen faster. The floor keeps the denoiser doing real smoothing even when
    // frameIndex is large but per-pixel variance is still high.
    constexpr float sigmaColorScale = 0.5f;
    constexpr float sigmaColorFloor = 0.1f;
    const float     adaptiveSigmaColor = std::max(sigmaColorScale / std::sqrt(static_cast<float>(ctx.frameIndex)), sigmaColorFloor);

    shader.use();
    shader.setFloat("sigma_color", adaptiveSigmaColor);
    for (int pass = 0; pass < 4; ++pass) {
        srcs[pass]->bind(0, GL_READ_ONLY);
        targets.normals.bind(1, GL_READ_ONLY);
        dsts[pass]->bind(2, GL_WRITE_ONLY);
        shader.setInt("step_size", steps[pass]);
        shader.setInt("apply_tonemapping", pass == 3 ? 1 : 0);
        glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
}
