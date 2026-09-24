#pragma once

/**
 * @file aov_pass.h
 * @brief Debug arbitrary-output-variable overlay, drawn over the final image.
 */

#include "gpu/compute_shader.h"
#include "render/render_pass.h"
#include "render/render_settings.h"

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
    /**
     * @brief Loads `aov.comp`.
     * @param settings Shared settings block; borrowed by reference and read every frame, so it
     *                 must outlive this pass.
     */
    explicit AovPass(const RenderSettings& settings);

    bool        reloadIfChanged() override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "AOV"; }

private:
    ComputeShader         shader;
    const RenderSettings& settings;
};
