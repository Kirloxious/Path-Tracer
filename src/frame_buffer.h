#pragma once

#include <glad/glad.h>

#include <vector>

#include "texture.h"

class FrameBuffer
{
public:
    GLuint handle = 0;
    int    numColorAttachments = 0;

    FrameBuffer() = default;
    explicit FrameBuffer(const Texture& texture);
    FrameBuffer(const std::vector<const Texture*>& colorAttachments, const Texture* depthAttachment);
    ~FrameBuffer();

    // Blits color attachment 0 to the default (swapchain) framebuffer.
    void blit(const Texture& texture, int dstWidth, int dstHeight) const;

    // Blits a specific color attachment to the default framebuffer.
    void blitAttachment(int attachmentIndex, int srcWidth, int srcHeight, int dstWidth, int dstHeight) const;

    // Non-copyable, movable
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&& o) noexcept;
    FrameBuffer& operator=(FrameBuffer&& o) noexcept;
};
