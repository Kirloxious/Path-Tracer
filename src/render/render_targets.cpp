#include "render/render_targets.h"

#include "gpu/frame_buffer.h"
#include "core/log.h"
#include "gpu/texture.h"

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

void RenderTargets::allocate(int w, int h) {
    // Only `accum` needs full float32: it carries the progressive running average across
    // thousands of frames, where half-float rounding would compound. Everything else is
    // either bounded HDR (rgba16f handles radiance far past anything a tonemap keeps) or
    // already tonemapped and sRGB-encoded into [0,1], which rgba8 stores exactly.
    // At 1080p this is ~231 MB of targets down to ~66 MB, and proportionally less traffic
    // in every post pass that reads or writes them.
    accum = Texture(w, h, GL_RGBA32F);
    normals = Texture(w, h, GL_RGBA16F);
    denoised_ping = Texture(w, h, GL_RGBA16F);
    hdr = Texture(w, h, GL_RGBA16F);
    // RGB10_A2, not RGBA8. These three carry tonemapped sRGB values in [0,1], which 8 bits
    // stores fine as a one-shot image — but taa_output feeds back into taa_history and is
    // re-blended every frame at up to 0.9 history weight, and 8-bit rounding inside that
    // loop never settles. 10 bits per channel costs the same 4 bytes and kills it.
    display = Texture(w, h, GL_RGB10_A2);
    taa_history = Texture(w, h, GL_RGB10_A2);
    taa_output = Texture(w, h, GL_RGB10_A2);
    gbuf = GBuffer(w, h);
    gbuf_prev = GBuffer(w, h);
    fb = FrameBuffer(display);

    // Both TAA textures get LINEAR filtering so that after each frame's swap (history
    // ↔ output) the incoming taa_history handle keeps bilinear reprojection working
    // without any per-frame glTextureParameteri fixup. Image writes ignore filter mode.
    for (const Texture* t : {&taa_history, &taa_output}) {
        glTextureParameteri(t->handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(t->handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    numGroupsX = (w + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    numGroupsY = (h + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
}
