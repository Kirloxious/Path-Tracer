#pragma once

#include "gpu/frame_buffer.h"
#include "gpu/texture.h"

class GBuffer
{
public:
    /// Shared by the raster shader and the debug blit.
    static constexpr int ATTACH_NORMAL = 0;

    /// xyz = world normal, w = material index (exact in a half float up to 2048).
    Texture normal;

    /// Reversed-Z: far = 0, near = 1. Also the source world position is reconstructed from.
    Texture depth;

    FrameBuffer fb;

    int width = 0;
    int height = 0;

    GBuffer() = default;

    GBuffer(int w, int h);

    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    GBuffer(GBuffer&&) noexcept = default;
    GBuffer& operator=(GBuffer&&) noexcept = default;

    void blitAttachmentToSwapChain(int attachmentIndex, int dstWidth, int dstHeight) const;
};
