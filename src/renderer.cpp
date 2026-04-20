#include "renderer.h"
#include "log.h"
#include <GL/glext.h>

// ----- Texture ---------------------------------------------------------------

Texture::Texture(int width, int height, GLenum internalFormat) : width(width), height(height), internalFormat(internalFormat) {
    glCreateTextures(GL_TEXTURE_2D, 1, &handle);
    glTextureStorage2D(handle, 1, internalFormat, width, height);
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

Texture::~Texture() {
    if (handle) {
        glDeleteTextures(1, &handle);
    }
}

Texture::Texture(Texture&& o) noexcept : handle(o.handle), width(o.width), height(o.height), internalFormat(o.internalFormat) {
    o.handle = 0;
}

Texture& Texture::operator=(Texture&& o) noexcept {
    if (this != &o) {
        if (handle) {
            glDeleteTextures(1, &handle);
        }
        handle = o.handle;
        width = o.width;
        height = o.height;
        internalFormat = o.internalFormat;
        o.handle = 0;
    }
    return *this;
}

void Texture::bindForAccumulation() const {
    glBindImageTexture(0, handle, 0, GL_FALSE, 0, GL_READ_WRITE, internalFormat);
}

void Texture::bind(int unit, GLenum access) const {
    glBindImageTexture(unit, handle, 0, GL_FALSE, 0, access, internalFormat);
}

// ----- FrameBuffer -----------------------------------------------------------

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