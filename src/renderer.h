#pragma once

#include <glad/glad.h>

class Texture
{
public:
    GLuint handle = 0;
    int    width = 0;
    int    height = 0;
    GLenum internalFormat = GL_RGBA32F;

    Texture() = default;
    Texture(int width, int height, GLenum internalFormat = GL_RGBA32F);
    ~Texture();

    void bindForAccumulation() const;
    void bind(int unit, GLenum access) const;

    // Non-copyable, movable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;
};

class FrameBuffer
{
public:
    GLuint handle = 0;

    FrameBuffer() = default;
    explicit FrameBuffer(const Texture& texture);
    ~FrameBuffer();

    // Blits this framebuffer to the default (swapchain) framebuffer.
    void blit(const Texture& texture, int dstWidth, int dstHeight) const;

    // Non-copyable, movable
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&& o) noexcept;
    FrameBuffer& operator=(FrameBuffer&& o) noexcept;
};