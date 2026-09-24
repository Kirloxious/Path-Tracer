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

    // A-Trous denoiser: 4 ping-pong passes, result HDR in targets.hdr — tonemap is a
    // downstream pass so bloom / auto-exposure can operate on the pre-tonemap image.
    const std::array<const Texture*, 4> srcs = {&targets.accum, &targets.denoised_ping, &targets.hdr, &targets.denoised_ping};
    const std::array<const Texture*, 4> dsts = {&targets.denoised_ping, &targets.hdr, &targets.denoised_ping, &targets.hdr};
    constexpr std::array<int, 4>        steps = {1, 2, 4, 8};

    // How many standard errors of the per-pixel luminance the colour edge-stop spans. The
    // estimate comes from resolve.comp's temporally accumulated moments, so the width adapts
    // per pixel rather than following one image-wide frame count — a converged region stops
    // filtering while a freshly disoccluded one beside it does not.
    //
    // Above 1 because the weight compares a *squared* difference against sigma squared, and the
    // difference of two independent means is itself sqrt(2) standard errors wide — at a scale
    // of 1 the filter would reject the very samples it exists to average and the image would
    // stop converging while still visibly grainy.
    constexpr float sigmaColorScale = 1.75f;

    shader.use();
    shader.setFloat("sigma_color_scale", sigmaColorScale);
    shader.setFloat("sigma_normal", 64.0f);
    // Plane tolerance per unit of view depth. At 1080p / 90° vfov the widest tap reaches
    // ~0.3 world units at depth 10, so the allowance has to stay well under that to keep a
    // wall out of the floor's filter.
    shader.setFloat("sigma_plane", 0.01f);
    // Variance inputs. Both describe the frame, not the ping-pong stage, so they are bound
    // once: `accum` carries the per-pixel history length in its alpha.
    targets.moments.bindSampler(3);
    targets.accum.bindSampler(4);

    for (std::size_t pass = 0; pass < steps.size(); ++pass) {
        // Source and normals are sampled, not image-bound: pass 0 reads `accum` (rgba32f)
        // and later passes read the rgba16f ping-pong pair, which a single image format
        // qualifier could not cover. Only the destination stays an image.
        srcs[pass]->bindSampler(0);
        targets.normals.bindSampler(1);
        dsts[pass]->bind(2, GL_WRITE_ONLY);
        shader.setInt("step_size", steps[pass]);
        // Pass 0's source is `accum`, whose alpha is the history length; from pass 1 on the
        // alpha is the variance the previous pass filtered, which is what this selects between.
        shader.setInt("first_pass", pass == 0 ? 1 : 0);
        GL::dispatch(targets.numGroupsX, targets.numGroupsY);
        // Written as an image, read back as a sampler — the fetch barrier is the one that
        // orders that, not the image-access bit alone.
        GL::memoryBarrier(GL::Barrier::ImageAccess | GL::Barrier::TextureFetch);
    }
}
