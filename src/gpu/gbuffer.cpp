#include "gpu/gbuffer.h"

GBuffer::GBuffer(int w, int h) : normal(w, h, GL_RGBA16F), depth(w, h, GL_DEPTH_COMPONENT32F), width(w), height(h) {
    fb = FrameBuffer({&normal}, &depth);
}

void GBuffer::blitAttachmentToSwapChain(int attachmentIndex, int dstWidth, int dstHeight) const {
    fb.blitAttachment(attachmentIndex, dstWidth, dstHeight);
}
