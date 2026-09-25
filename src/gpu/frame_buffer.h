#pragma once

#include <glad/glad.h>

#include <vector>

#include "gpu/gl_handle.h"
#include "gpu/texture.h"

/// Borrows its attachments, which must outlive it. An incomplete FBO is logged and leaves id() at 0.
class FrameBuffer
{
public:
    int numColorAttachments = 0;
    /// blit() reads from this FBO, so the source rect must come from its own attachments, not a
    /// caller-supplied texture.
    int width = 0;
    int height = 0;

    FrameBuffer() = default;

    explicit FrameBuffer(const Texture& texture);

    /// `width`/`height` come from the first colour attachment.
    FrameBuffer(const std::vector<const Texture*>& colorAttachments, const Texture* depthAttachment);

    void blit(int dstWidth, int dstHeight) const;

    void blitAttachment(int attachmentIndex, int dstWidth, int dstHeight) const;

    void bind() const;

    static void bindDefault();

    void clearColor(int attachmentIndex, const float* rgba) const;

    void clearDepth(float depth) const;

    [[nodiscard]] GLuint id() const { return m_handle.get(); }

private:
    FramebufferHandle m_handle;
};
