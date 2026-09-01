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
    accum = Texture(w, h);
    normals = Texture(w, h, GL_RGBA16F);
    denoised_ping = Texture(w, h);
    hdr = Texture(w, h);
    display = Texture(w, h);
    taa_history = Texture(w, h);
    taa_output = Texture(w, h);
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
