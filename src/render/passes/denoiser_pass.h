#pragma once

/**
 * @file denoiser_pass.h
 * @brief A-Trous edge-aware denoiser over the path tracer's running average.
 */

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/**
 * @brief Four ping-pong dispatches of an A-Trous bilateral filter, writing `targets.hdr`.
 *
 * Step sizes double each pass (1, 2, 4, 8). Edge stops are luminance distance, normal
 * similarity and distance to the centre tap's tangent plane. `sigma_color` is sized from a
 * per-pixel variance estimate that each pass filters alongside the colour and hands to the
 * next in its alpha, so the wide taps size their edge-stop for the noise still present rather
 * than for the noise the first pass already removed.
 */
class DenoiserPass : public RenderPass
{
public:
    /**
     * @brief Loads the denoiser kernel.
     * @param shaderPath Path to `denoiser.comp`.
     */
    explicit DenoiserPass(const std::filesystem::path& shaderPath);

    bool        reloadIfChanged() override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Denoiser"; }

private:
    ComputeShader shader;
};
