#include "render_targets.h"

#include "frame_buffer.h"
#include "log.h"
#include "texture.h"

RenderTargets::RenderTargets(int w, int h) : accum(w, h), normals(w, h, GL_RGBA16F), denoised_ping(w, h), display(w, h), gbuf(w, h), fb() {

    fb = FrameBuffer(display);
    numGroupsX = (w + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    numGroupsY = (h + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    Log::info("Render targets: {}x{} — dispatch {}x{} groups of {}x{}", w, h, numGroupsX, numGroupsY, WORK_GROUP_SIZE, WORK_GROUP_SIZE);
}

void RenderTargets::resize(int w, int h) {
    (void)w;
    (void)h;
}
