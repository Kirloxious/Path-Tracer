#include "frame_buffer.h"

#include <GL/glext.h>

#include "log.h"

FrameBuffer::FrameBuffer(const Texture& texture) {
    glCreateFramebuffers(1, &handle);
    glNamedFramebufferTexture(handle, GL_COLOR_ATTACHMENT0, texture.handle, 0);
    numColorAttachments = 1;

    if (glCheckNamedFramebufferStatus(handle, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log::error("Framebuffer is not complete");
        glDeleteFramebuffers(1, &handle);
        handle = 0;
    }
}

FrameBuffer::FrameBuffer(const std::vector<const Texture*>& colorAttachments, const Texture* depthAttachment) {
    glCreateFramebuffers(1, &handle);

    std::vector<GLenum> drawBufs;
    drawBufs.reserve(colorAttachments.size());
    for (size_t i = 0; i < colorAttachments.size(); ++i) {
        const Texture* tex = colorAttachments[i];
        const GLenum   attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        glNamedFramebufferTexture(handle, attachment, tex->handle, 0);
        drawBufs.push_back(attachment);
    }
    numColorAttachments = static_cast<int>(colorAttachments.size());

    if (depthAttachment) {
        glNamedFramebufferTexture(handle, GL_DEPTH_ATTACHMENT, depthAttachment->handle, 0);
    }

    glNamedFramebufferDrawBuffers(handle, static_cast<GLsizei>(drawBufs.size()), drawBufs.data());

    if (glCheckNamedFramebufferStatus(handle, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log::error("Framebuffer is not complete (multi-attachment)");
        glDeleteFramebuffers(1, &handle);
        handle = 0;
    }
}

FrameBuffer::~FrameBuffer() {
    if (handle) {
        glDeleteFramebuffers(1, &handle);
    }
}

FrameBuffer::FrameBuffer(FrameBuffer&& o) noexcept : handle(o.handle), numColorAttachments(o.numColorAttachments) {
    o.handle = 0;
    o.numColorAttachments = 0;
}

FrameBuffer& FrameBuffer::operator=(FrameBuffer&& o) noexcept {
    if (this != &o) {
        if (handle) {
            glDeleteFramebuffers(1, &handle);
        }
        handle = o.handle;
        numColorAttachments = o.numColorAttachments;
        o.handle = 0;
        o.numColorAttachments = 0;
    }
    return *this;
}

void FrameBuffer::blit(const Texture& texture, int dstWidth, int dstHeight) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, handle);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // swapchain
    glNamedFramebufferReadBuffer(handle, GL_COLOR_ATTACHMENT0);
    // clang-format off
    glBlitFramebuffer(0, 0, texture.width, texture.height,   // source rect
                      0, 0, dstWidth, dstHeight,             // destination rect
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    // clang-format on
}

void FrameBuffer::blitAttachment(int attachmentIndex, int srcWidth, int srcHeight, int dstWidth, int dstHeight) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, handle);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glNamedFramebufferReadBuffer(handle, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + attachmentIndex));
    glBlitFramebuffer(0, 0, srcWidth, srcHeight, 0, 0, dstWidth, dstHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}
