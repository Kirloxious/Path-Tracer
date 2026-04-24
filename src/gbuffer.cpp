#include "gbuffer.h"

GBuffer::GBuffer(int w, int h)
    : pos_matid(w, h, GL_RGBA32F), normal(w, h, GL_RGBA16F), motion(w, h, GL_RG16F), depth(w, h, GL_DEPTH_COMPONENT32F), width(w), height(h) {
    fb = FrameBuffer({&pos_matid, &normal, &motion}, &depth);
}

void GBuffer::blitAttachmentToSwapChain(int attachmentIndex, int dstWidth, int dstHeight) const {
    fb.blitAttachment(attachmentIndex, width, height, dstWidth, dstHeight);
}
