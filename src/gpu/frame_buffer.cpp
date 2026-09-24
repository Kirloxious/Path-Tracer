#include "gpu/frame_buffer.h"

#include "core/log.h"

namespace {
FramebufferHandle createFramebuffer() {
    GLuint id = 0;
    glCreateFramebuffers(1, &id);
    return FramebufferHandle(id);
}
} // namespace

FrameBuffer::FrameBuffer(const Texture& texture) : width(texture.width), height(texture.height), m_handle(createFramebuffer()) {
    glNamedFramebufferTexture(id(), GL_COLOR_ATTACHMENT0, texture.id(), 0);
    numColorAttachments = 1;

    if (glCheckNamedFramebufferStatus(id(), GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log::error("Framebuffer is not complete");
        m_handle.reset();
    }
}

FrameBuffer::FrameBuffer(const std::vector<const Texture*>& colorAttachments, const Texture* depthAttachment) : m_handle(createFramebuffer()) {
    if (!colorAttachments.empty()) {
        width = colorAttachments.front()->width;
        height = colorAttachments.front()->height;
    }

    std::vector<GLenum> drawBufs;
    drawBufs.reserve(colorAttachments.size());
    for (size_t i = 0; i < colorAttachments.size(); ++i) {
        const GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        glNamedFramebufferTexture(id(), attachment, colorAttachments[i]->id(), 0);
        drawBufs.push_back(attachment);
    }
    numColorAttachments = static_cast<int>(colorAttachments.size());

    if (depthAttachment) {
        glNamedFramebufferTexture(id(), GL_DEPTH_ATTACHMENT, depthAttachment->id(), 0);
    }

    glNamedFramebufferDrawBuffers(id(), static_cast<GLsizei>(drawBufs.size()), drawBufs.data());

    if (glCheckNamedFramebufferStatus(id(), GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log::error("Framebuffer is not complete (multi-attachment)");
        m_handle.reset();
    }
}

void FrameBuffer::blit(int dstWidth, int dstHeight) const {
    blitAttachment(0, dstWidth, dstHeight);
}

void FrameBuffer::blitAttachment(int attachmentIndex, int dstWidth, int dstHeight) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, id());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glNamedFramebufferReadBuffer(id(), static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + attachmentIndex));
    glBlitFramebuffer(0, 0, width, height, 0, 0, dstWidth, dstHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}
