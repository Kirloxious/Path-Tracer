#pragma once

/**
 * @file render_targets.h
 * @brief Every intermediate image the pass chain shares, plus dispatch tiling constants.
 */

#include <gpu/frame_buffer.h>
#include <gpu/texture.h>

#include "gpu/gbuffer.h"

class EnvMap;

/**
 * @brief Shared ownership of all intermediate render targets, passed to every pass's execute().
 *
 * Passes communicate through these textures rather than through direct references to each
 * other, so reordering the chain is a matter of who reads and writes what. Owned by Renderer
 * and reallocated wholesale on resize.
 */
struct RenderTargets
{
    /// Compute dispatch tile size; `numGroupsX/Y` are derived from it.
    static constexpr int WORK_GROUP_SIZE = 8;

    Texture accum;         ///< Path tracer output — the running-average radiance image.
    Texture normals;       ///< Primary normals + material type, consumed by the denoiser.
    Texture denoised_ping; ///< A-Trous ping-pong scratch target.
    Texture hdr;           ///< HDR pre-tonemap image — output of the denoiser, modified by bloom, read by auto-exposure + tonemap.
    Texture display;       ///< Final LDR image blitted to the swap chain.
    Texture taa_history;   ///< Previous frame's TAA-resolved LDR image (sampled bilinear for reprojection).
    Texture taa_output;    ///< Scratch target for this frame's TAA write; copied into `display` and swapped into `taa_history`.

    /// Current frame's primary-visibility data.
    ///
    /// `gbuf_prev` holds the previous frame's, used for temporal reprojection (ReSTIR temporal
    /// reuse, motion-vector validation). RasterGBufferPass swaps the pair every frame so
    /// downstream code can always read `gbuf` as "current" and `gbuf_prev` as "last frame".
    GBuffer gbuf;
    /// Previous frame's primary-visibility data. See gbuf.
    GBuffer gbuf_prev;

    FrameBuffer fb; ///< Wraps `display`, used for the final swap-chain blit.

    /// Optional per-scene HDR envmap. Owned by Renderer, which sets this pointer in
    /// loadScene() (null if the scene has no envmap). Passes read it in execute().
    const EnvMap* envMap = nullptr;

    GLuint numGroupsX = 0; ///< ceil(width  / WORK_GROUP_SIZE), for 8x8 per-pixel dispatches.
    GLuint numGroupsY = 0; ///< ceil(height / WORK_GROUP_SIZE).

    /**
     * @brief Allocates every target at @p w x @p h.
     * @param w Width in pixels.
     * @param h Height in pixels.
     */
    RenderTargets(int w, int h);

    /**
     * @brief Reallocates every target for a new framebuffer size.
     *
     * All contents are lost, including accumulated radiance and both G-buffers — the caller
     * must reset `frameIndex` so accumulation restarts.
     *
     * @param w New width in pixels.
     * @param h New height in pixels.
     */
    void resize(int w, int h);

private:
    /// Single allocation path shared by the constructor and resize().
    void allocate(int w, int h);
};
