#include "frame_buffer.h"

#include <GL/glext.h>

#include "log.h"

FrameBuffer::FrameBuffer(const Texture& texture) {
    glCreateFramebuffers(1, &handle);
    glNamedFramebufferTexture(handle, GL_COLOR_ATTACHMENT0, texture.handle, 0);

    if (glCheckNamedFramebufferStatus(handle, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log::error("Framebuffer is not complete");
        glDeleteFramebuffers(1, &handle);
        handle = 0;
    }
}

FrameBuffer::~FrameBuffer() {
    if (handle) {
        glDeleteFramebuffers(1, &handle);
    }
}

FrameBuffer::FrameBuffer(FrameBuffer&& o) noexcept : handle(o.handle) {
    o.handle = 0;
}

FrameBuffer& FrameBuffer::operator=(FrameBuffer&& o) noexcept {
    if (this != &o) {
        if (handle) {
            glDeleteFramebuffers(1, &handle);
        }
        handle = o.handle;
        o.handle = 0;
    }
    return *this;
}

void FrameBuffer::blit(const Texture& texture, int dstWidth, int dstHeight) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, handle);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // swapchain
    // clang-format off
    glBlitFramebuffer(0, 0, texture.width, texture.height,   // source rect
                      0, 0, dstWidth, dstHeight,             // destination rect
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    // clang-format on
}
