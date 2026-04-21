#include "render_targets.h"
RenderTargets::RenderTargets(int w, int h) {
    accum = Texture(w, h);
    normals = Texture(w, h, GL_RGBA16F);
    denoised_ping = Texture(w, h);
    display = Texture(w, h);
    fb = FrameBuffer(display);

    numGroupsX = (w + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    numGroupsY = (h + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
}

void RenderTargets::resize(int w, int h) {}
