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
 * Reads `targets.tonemapped` (this frame's tonemapped image) and `targets.taa_history` (last
 * frame's TAA output, sampled bilinearly), reprojects via the camera UBO's un-jittered
 * `prev_view_proj` — the primary hit's world position, or the view direction for sky —
 * clips history against the 3x3 YCoCg neighbourhood to suppress ghosting, and blends.
 *
 * The result is written twice: into `targets.display` for downstream passes (AOV, the swap-chain
 * blit), and at half-float precision into `targets.taa_output`, which RenderTargets::endFrame()
 * makes next frame's `taa_history`.
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
