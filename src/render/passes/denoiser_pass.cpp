#include "render/passes/denoiser_pass.h"

#include <array>

#include "gpu/compute_shader.h"
#include "core/log.h"
#include "core/shader_shared.h"
#include "gpu/gl.h"
#include "render/render_pass.h"

DenoiserPass::DenoiserPass(const std::filesystem::path& shaderPath) {
    Log::info("DenoiserPass: loading '{}'", shaderPath.string());
    shader = ComputeShader(shaderPath);
}

bool DenoiserPass::reloadIfChanged() {
    return shader.reloadIfChanged();
}

void DenoiserPass::execute(const RenderContext&, RenderTargets& targets) {

    const std::array<const Texture*, 4> srcs = {&targets.accum, &targets.denoised_ping, &targets.hdr, &targets.denoised_ping};
    const std::array<const Texture*, 4> dsts = {&targets.denoised_ping, &targets.hdr, &targets.denoised_ping, &targets.hdr};
    constexpr std::array<int, 4>        steps = {1, 2, 4, 8};

    // Standard errors the colour edge-stop spans. Above 1 because the difference of two independent
    // means is itself sqrt(2) standard errors wide; at 1 the filter rejects the samples it should average.
    constexpr float sigmaColorScale = 1.75f;

    shader.use();
    shader.setFloat("sigma_color_scale", sigmaColorScale);
    shader.setFloat("sigma_normal", 64.0f);
    // Per unit view depth. The widest tap reaches ~0.3 units at depth 10 (1080p, 90° vfov), so this
    // must stay well under that to keep walls out of the floor's filter.
    shader.setFloat("sigma_plane", 0.01f);
    // Bound once: both describe the frame, not the ping-pong stage.
    targets.moments.bindSampler(3);
    targets.accum.bindSampler(4);

    for (std::size_t pass = 0; pass < steps.size(); ++pass) {
        // Sampled, not image-bound: pass 0 reads rgba32f `accum`, later passes rgba16f, which one image
        // format qualifier can't cover.
        srcs[pass]->bindSampler(0);
        targets.normals.bindSampler(1);
        dsts[pass]->bind(2, GL_WRITE_ONLY);
        shader.setInt("step_size", steps[pass]);
        // Pass 0's alpha is the history length; later passes' alpha is the filtered variance.
        shader.setInt("first_pass", pass == 0 ? 1 : 0);
        GL::dispatch(targets.numGroupsX, targets.numGroupsY);
        // Written as an image, read as a sampler: needs the fetch barrier, not just image access.
        GL::memoryBarrier(GL::Barrier::ImageAccess | GL::Barrier::TextureFetch);
    }
}
