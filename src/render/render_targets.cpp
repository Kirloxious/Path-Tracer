#include "render/render_targets.h"

#include "gpu/frame_buffer.h"
#include "core/log.h"
#include "gpu/texture.h"

RenderTargets::RenderTargets(int w, int h)
    : accum(w, h), normals(w, h, GL_RGBA16F), denoised_ping(w, h), hdr(w, h), display(w, h), gbuf(w, h), gbuf_prev(w, h), fb() {

    fb = FrameBuffer(display);
    numGroupsX = (w + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    numGroupsY = (h + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    Log::info("Render targets: {}x{} — dispatch {}x{} groups of {}x{}", w, h, numGroupsX, numGroupsY, WORK_GROUP_SIZE, WORK_GROUP_SIZE);
}

void RenderTargets::resize(int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    accum = Texture(w, h);
    normals = Texture(w, h, GL_RGBA16F);
    denoised_ping = Texture(w, h);
    hdr = Texture(w, h);
    display = Texture(w, h);
    gbuf = GBuffer(w, h);
    gbuf_prev = GBuffer(w, h);
    fb = FrameBuffer(display);

    numGroupsX = (w + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    numGroupsY = (h + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    Log::info("Render targets resized: {}x{} — dispatch {}x{} groups of {}x{}", w, h, numGroupsX, numGroupsY, WORK_GROUP_SIZE, WORK_GROUP_SIZE);
}
