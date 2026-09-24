#pragma once

/**
 * @file aov_pass.h
 * @brief Debug arbitrary-output-variable overlay, drawn over the final image.
 */

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/**
 * @brief Overwrites the display texture with a debug visualization of an intermediate buffer.
 *
 * Runs after the denoiser and, when `settings.aovMode != None`, replaces `targets.display`
 * with world normals, linear depth, albedo, material ID, a BVH-traversal-cost heatmap or
 * per-pixel variance.
 *
 * When the mode is None the pass early-exits without dispatching, so the tonemapped image
 * reaches the swap chain untouched.
 */
class AovPass : public RenderPass
{
public:
    /// Loads `aov.comp`.
    AovPass();

    bool             reloadIfChanged() override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "AOV"; }

private:
    ComputeShader shader;
};
