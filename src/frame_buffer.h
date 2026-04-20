#pragma once

#include <glad/glad.h>

#include "texture.h"

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
