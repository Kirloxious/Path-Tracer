#include "render/passes/denoiser_pass.h"

#include <algorithm>
#include <cmath>

#include "gpu/compute_shader.h"
#include "core/log.h"
#include "render/render_pass.h"

DenoiserPass::DenoiserPass(const std::filesystem::path& shaderPath) {
    Log::info("DenoiserPass: loading '{}'", shaderPath.string());
    shader = ComputeShader(shaderPath);
}

void DenoiserPass::uploadUniforms(const Scene&, const Camera& camera) {
    shader.use();
    shader.setIVec2("image_size", camera.image_width, camera.image_height);
    shader.setFloat("sigma_normal", 64.0f);
}

bool DenoiserPass::reloadIfChanged(const RenderContext&) {
    return shader.reloadIfChanged();
}

void DenoiserPass::resize(int w, int h) {
    shader.use();
    shader.setIVec2("image_size", w, h);
}

void DenoiserPass::execute(const RenderContext& ctx, RenderTargets& targets) {

    // A-Trous denoiser: 4 ping-pong passes, result HDR in targets.hdr — tonemap is a
    // downstream pass so bloom / auto-exposure can operate on the pre-tonemap image.
    Texture* srcs[4] = {&targets.accum, &targets.denoised_ping, &targets.hdr, &targets.denoised_ping};
    Texture* dsts[4] = {&targets.denoised_ping, &targets.hdr, &targets.denoised_ping, &targets.hdr};
    int      steps[4] = {1, 2, 4, 8};

    // Tighter than noise-stddev (1/sqrt(N)) so shadow penumbras — which live on
    // same-normal surfaces and rely solely on the color weight to stop the filter —
    // sharpen faster. The floor keeps the denoiser doing real smoothing even when
    // frameIndex is large but per-pixel variance is still high.
    constexpr float sigmaColorScale = 0.5f;
    constexpr float sigmaColorFloor = 0.1f;
    // frameIndex is 0 on the frame a shader reload restarts accumulation; sqrt(0) there
    // would hand the shader an infinite sigma, which stops nothing and blurs the whole image.
    const float samples = static_cast<float>(std::max(ctx.frameIndex, 1));
    const float adaptiveSigmaColor = std::max(sigmaColorScale / std::sqrt(samples), sigmaColorFloor);

    shader.use();
    shader.setFloat("sigma_color", adaptiveSigmaColor);
    for (int pass = 0; pass < 4; ++pass) {
        // Source and normals are sampled, not image-bound: pass 0 reads `accum` (rgba32f)
        // and later passes read the rgba16f ping-pong pair, which a single image format
        // qualifier could not cover. Only the destination stays an image.
        glBindTextureUnit(0, srcs[pass]->handle);
        glBindTextureUnit(1, targets.normals.handle);
        dsts[pass]->bind(2, GL_WRITE_ONLY);
        shader.setInt("step_size", steps[pass]);
        glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
}
