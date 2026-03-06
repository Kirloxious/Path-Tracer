#pragma once

#include <glad/glad.h>

class Texture
{
public:
    GLuint handle = 0;
    int    width = 0;
    int    height = 0;

    Texture() = default;
    Texture(int width, int height);
    ~Texture();

    // Binds this texture to image units 0 (read) and 1 (write) for frame accumulation.
    void bindAsDoubleBuffer() const;

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
    void blit(const Texture& texture) const;

    // Non-copyable, movable
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&& o) noexcept;
    FrameBuffer& operator=(FrameBuffer&& o) noexcept;
};