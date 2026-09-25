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
    std::swap(taa_output, taa_history);
}

void RenderTargets::allocate(int w, int h) {
    // Only `accum` needs float32: its running average spans thousands of frames, where half-float
    // rounding would compound.
    accum = Texture(w, h, GL_RGBA32F);
    moments = Texture(w, h, GL_RG32F);
    normals = Texture(w, h, GL_RGBA16F);
    denoised_ping = Texture(w, h, GL_RGBA16F);
    hdr = Texture(w, h, GL_RGBA16F);
    tonemapped = Texture(w, h, GL_RGB10_A2);
    display = Texture(w, h, GL_RGB10_A2);
    // Half float: history is re-blended at up to 0.9 weight, and at 10 bits a step under ~5 codes
    // rounds back to the stored value, so it could never settle.
    taa_history = Texture(w, h, GL_RGBA16F);
    taa_output = Texture(w, h, GL_RGBA16F);
    gbuf = GBuffer(w, h);
    gbuf_prev = GBuffer(w, h);
    fb = FrameBuffer(display);

    // Both TAA textures are LINEAR so bilinear reprojection survives the per-frame history swap.
    taa_history.setFilter(GL_LINEAR);
    taa_output.setFilter(GL_LINEAR);

    width = w;
    height = h;
    numGroupsX = (w + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    numGroupsY = (h + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
}
