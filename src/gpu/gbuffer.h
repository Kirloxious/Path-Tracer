#pragma once

/**
 * @file gbuffer.h
 * @brief Primary-visibility G-buffer written by the raster pass.
 */

#include "gpu/frame_buffer.h"
#include "gpu/texture.h"

/**
 * @brief Three MRT targets plus their FBO, holding one frame of primary visibility.
 *
 * RasterGBufferPass fills these each frame; the path tracer samples them in place of casting
 * primary rays, and ReSTIR's temporal kernel samples the *previous* frame's pair for
 * reprojection. RenderTargets keeps two GBuffers and swaps them every frame.
 *
 * Non-copyable, movable (the attachments are Textures, which are themselves move-only).
 */
class GBuffer
{
public:
    /// Attachment-index constants kept in one place so the raster shader,
    /// the debug blit, and the consumer (path tracer) can't drift.
    static constexpr int ATTACH_NORMAL = 0;

    /// rgba16f — xyz = world-space surface normal (normalized), w = material index.
    ///
    /// The material index rides in .w because a half float stores integers exactly up to
    /// 2048, far more material slots than any scene here uses. It used to live in the .w of
    /// a separate rgba32f world-position target, which is gone: world position is now
    /// reconstructed from `depth` (see common/gbuffer.glsl).
    Texture normal;

    /// Hardware depth — depth testing during the raster pass, and the source world position
    /// is reconstructed from. Reversed-Z, so the far plane is 0 and the near plane is 1;
    /// see makeReversedZProjection() in camera.cpp for why.
    Texture depth;

    FrameBuffer fb;

    int width = 0;
    int height = 0;

    GBuffer() = default;

    /**
     * @brief Allocates both attachments at @p w x @p h and builds the framebuffer.
     * @param w Width in pixels.
     * @param h Height in pixels.
     */
    GBuffer(int w, int h);

    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    GBuffer(GBuffer&&) noexcept = default;
    GBuffer& operator=(GBuffer&&) noexcept = default;

    /**
     * @brief Blits one attachment to the default framebuffer for debug visualization.
     * @param attachmentIndex ATTACH_NORMAL (the only colour attachment).
     * @param dstWidth        Destination width in pixels.
     * @param dstHeight       Destination height in pixels.
     */
    void blitAttachmentToSwapChain(int attachmentIndex, int dstWidth, int dstHeight) const;
};
