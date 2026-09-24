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
 * downstream passes (AOV, the swap-chain blit) see the resolved image. RenderTargets::endFrame()
 * then makes `taa_output` next frame's `taa_history`.
 */
class TaaPass : public RenderPass
{
public:
    /// Loads `taa.comp`.
    TaaPass();

    bool             reloadIfChanged() override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "TAA"; }

private:
    ComputeShader shader;
};
