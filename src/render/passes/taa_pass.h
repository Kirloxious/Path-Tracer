#pragma once

/**
 * @file taa_pass.h
 * @brief Temporal anti-aliasing resolve over the tonemapped image.
 */

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/**
 * @brief Temporal anti-aliasing with neighborhood clamping.
 *
 * Reads `targets.display` (this frame's tonemapped image) and `targets.taa_history` (last
 * frame's TAA output, sampled bilinearly), reprojects via the camera UBO's un-jittered
 * `prev_view_proj` using the primary hit's world position from the G-buffer, applies a 3x3 RGB
 * neighborhood clamp to suppress ghosting, and blends.
 *
 * The result is written into `targets.taa_output`, then copied back into `targets.display` so
 * downstream passes (AOV, the swap-chain blit) see the resolved image. `taa_output` and
 * `taa_history` are then swapped, making this frame's result next frame's history.
 */
class TaaPass : public RenderPass
{
public:
    /**
     * @brief Loads `taa.comp` and caches the dispatch dimensions.
     * @param width  Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     */
    TaaPass(int width, int height);

    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "TAA"; }

private:
    int           width;
    int           height;
    ComputeShader shader;
};
