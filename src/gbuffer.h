#pragma once

#include "frame_buffer.h"
#include "texture.h"

// Raster primary-visibility pass writes into these targets each frame.
// The path tracer will later sample them in place of casting primary rays.
class GBuffer
{
public:
    // Attachment-index constants kept in one place so the raster shader,
    // the debug blit, and the consumer (path tracer) can't drift.
    static constexpr int ATTACH_POS_MATID = 0;
    static constexpr int ATTACH_NORMAL = 1;
    static constexpr int ATTACH_MOTION = 2;

    // xyz = world-space hit position, w = floatBitsToUint(material_index)
    Texture pos_matid;
    // xyz = world-space surface normal (normalized)
    Texture normal;
    // rg  = screen-space motion vector in NDC units (unused in phase 1)
    Texture motion;
    // hardware depth — enables depth test during the raster pass
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

    // Blit a chosen color attachment to the default framebuffer (for debug viz).
    void blitAttachmentToSwapChain(int attachmentIndex, int dstWidth, int dstHeight) const;
};
