#include "render/render_targets.h"

#include "gpu/frame_buffer.h"
#include "core/log.h"
#include "gpu/texture.h"

#include <utility>

RenderTargets::RenderTargets(int w, int h) {
    allocate(w, h);
    Log::info("Render targets: {}x{} — dispatch {}x{} groups of {}x{}", w, h, numGroupsX, numGroupsY, WORK_GROUP_SIZE, WORK_GROUP_SIZE);
}

void RenderTargets::resize(int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    allocate(w, h);
    Log::info("Render targets resized: {}x{} — dispatch {}x{} groups of {}x{}", w, h, numGroupsX, numGroupsY, WORK_GROUP_SIZE, WORK_GROUP_SIZE);
}

void RenderTargets::beginFrame() {
    std::swap(gbuf, gbuf_prev);
}

void RenderTargets::endFrame() {
    // Both TAA textures use LINEAR filtering (see allocate()), so the swap keeps bilinear
    // reprojection working without re-setting sampler state.
    std::swap(taa_output, taa_history);
}

void RenderTargets::allocate(int w, int h) {
    // Only `accum` needs full float32: it carries the progressive running average across
    // thousands of frames, where half-float rounding would compound. Everything else is
    // either bounded HDR (rgba16f handles radiance far past anything a tonemap keeps) or
    // already tonemapped and sRGB-encoded into [0,1], which rgba8 stores exactly.
    // At 1080p this is ~231 MB of targets down to ~66 MB, and proportionally less traffic
    // in every post pass that reads or writes them.
    accum = Texture(w, h, GL_RGBA32F);
    moments = Texture(w, h, GL_RG32F);
    normals = Texture(w, h, GL_RGBA16F);
    denoised_ping = Texture(w, h, GL_RGBA16F);
    hdr = Texture(w, h, GL_RGBA16F);
    tonemapped = Texture(w, h, GL_RGB10_A2);
    display = Texture(w, h, GL_RGB10_A2);
    // Half float, not the 10-bit unorm of the one-shot targets: history is re-blended every
    // frame at up to 0.9 weight, and at 10 bits any step under ~5 codes rounds back to the
    // stored value, so it can never settle closer than that to the current frame.
    taa_history = Texture(w, h, GL_RGBA16F);
    taa_output = Texture(w, h, GL_RGBA16F);
    gbuf = GBuffer(w, h);
    gbuf_prev = GBuffer(w, h);
    fb = FrameBuffer(display);

    // Both TAA textures get LINEAR filtering so that after each frame's swap (history
    // ↔ output) the incoming taa_history keeps bilinear reprojection working without any
    // per-frame fixup. Image writes ignore filter mode.
    taa_history.setFilter(GL_LINEAR);
    taa_output.setFilter(GL_LINEAR);

    width = w;
    height = h;
    numGroupsX = (w + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    numGroupsY = (h + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
}
