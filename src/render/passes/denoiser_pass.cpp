#include "render/passes/denoiser_pass.h"

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
    // Plane tolerance per unit of view depth. At 1080p / 90° vfov the widest tap reaches
    // ~0.3 world units at depth 10, so the allowance has to stay well under that to keep a
    // wall out of the floor's filter; the kernel hot-reloads, so this is worth scrubbing.
    shader.setFloat("sigma_plane", 0.01f);
}

bool DenoiserPass::reloadIfChanged(const RenderContext&) {
    return shader.reloadIfChanged();
}

void DenoiserPass::resize(int w, int h) {
    shader.use();
    shader.setIVec2("image_size", w, h);
}

void DenoiserPass::execute(const RenderContext&, RenderTargets& targets) {

    // A-Trous denoiser: 4 ping-pong passes, result HDR in targets.hdr — tonemap is a
    // downstream pass so bloom / auto-exposure can operate on the pre-tonemap image.
    Texture* srcs[4] = {&targets.accum, &targets.denoised_ping, &targets.hdr, &targets.denoised_ping};
    Texture* dsts[4] = {&targets.denoised_ping, &targets.hdr, &targets.denoised_ping, &targets.hdr};
    int      steps[4] = {1, 2, 4, 8};

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
    // Depth feeds the plane edge-stop's view-space reconstruction. Bound here rather than
    // inherited from an earlier pass: nothing in the pass contract says a pass in between
    // leaves unit 10 alone.
    glBindTextureUnit(10, targets.gbuf.depth.handle);
    // Variance inputs. Both describe the frame, not the ping-pong stage, so they are bound
    // once: `accum` carries the per-pixel history length in its alpha.
    glBindTextureUnit(3, targets.moments.handle);
    glBindTextureUnit(4, targets.accum.handle);

    for (int pass = 0; pass < 4; ++pass) {
        // Source and normals are sampled, not image-bound: pass 0 reads `accum` (rgba32f)
        // and later passes read the rgba16f ping-pong pair, which a single image format
        // qualifier could not cover. Only the destination stays an image.
        glBindTextureUnit(0, srcs[pass]->handle);
        glBindTextureUnit(1, targets.normals.handle);
        dsts[pass]->bind(2, GL_WRITE_ONLY);
        shader.setInt("step_size", steps[pass]);
        // Pass 0's source is `accum`, whose alpha is the history length; from pass 1 on the
        // alpha is the variance the previous pass filtered, which is what this selects between.
        shader.setInt("first_pass", pass == 0 ? 1 : 0);
        glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
        // Written as an image, read back as a sampler — the fetch barrier is the one that
        // orders that, not the image-access bit alone.
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }
}
