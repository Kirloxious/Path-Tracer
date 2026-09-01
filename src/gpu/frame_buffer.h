#pragma once

/**
 * @file frame_buffer.h
 * @brief RAII framebuffer object with swap-chain blit helpers.
 */

#include <glad/glad.h>

#include <vector>

#include "gpu/texture.h"

/**
 * @brief Owns one FBO wrapping caller-supplied textures.
 *
 * The FBO holds only references to its attachments — the Texture objects must outlive it.
 * An incomplete framebuffer is reported through Log::error and leaves `handle` at 0 rather
 * than throwing.
 *
 * Non-copyable, movable.
 */
class FrameBuffer
{
public:
    GLuint handle = 0;
    int    numColorAttachments = 0;
    /// Dimensions of the attachments, captured at construction. blit() reads from this
    /// FBO, so the source rect must come from here — taking it from a caller-supplied
    /// texture let a size mismatch silently blit the wrong region.
    int width = 0;
    int height = 0;

    FrameBuffer() = default;

    /**
     * @brief Single-attachment FBO: @p texture becomes `GL_COLOR_ATTACHMENT0`.
     * @param texture Colour target; must outlive this FrameBuffer.
     */
    explicit FrameBuffer(const Texture& texture);

    /**
     * @brief Multiple-render-target FBO with an optional depth attachment.
     *
     * Colour textures are attached in order starting at `GL_COLOR_ATTACHMENT0` and enabled
     * as draw buffers. `width`/`height` are taken from the first colour attachment.
     *
     * @param colorAttachments Colour targets in attachment order; each must outlive this FBO.
     * @param depthAttachment  Optional depth target, or nullptr for no depth attachment.
     */
    FrameBuffer(const std::vector<const Texture*>& colorAttachments, const Texture* depthAttachment);
    ~FrameBuffer();

    /**
     * @brief Blits colour attachment 0 to the default (swap-chain) framebuffer.
     * @param dstWidth  Destination width in pixels — scaling is applied if it differs.
     * @param dstHeight Destination height in pixels.
     */
    void blit(int dstWidth, int dstHeight) const;

    /**
     * @brief Blits one specific colour attachment to the default framebuffer.
     * @param attachmentIndex Index into the colour attachments (0-based).
     * @param dstWidth        Destination width in pixels.
     * @param dstHeight       Destination height in pixels.
     */
    void blitAttachment(int attachmentIndex, int dstWidth, int dstHeight) const;

    // Non-copyable, movable
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&& o) noexcept;
    FrameBuffer& operator=(FrameBuffer&& o) noexcept;
};
