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
 * Step sizes double each pass (1, 2, 4, 8). Edge stops are colour distance and normal
 * similarity; `sigma_color` decays with the accumulation frame count and floors out, so
 * penumbras keep sharpening as the image converges rather than staying permanently blurred.
 */
class DenoiserPass : public RenderPass
{
public:
    /**
     * @brief Loads the denoiser kernel.
     * @param shaderPath Path to `denoiser.comp`.
     */
    explicit DenoiserPass(const std::filesystem::path& shaderPath);

    void        uploadUniforms(const Scene&, const Camera&) override;
    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Denoiser"; }

private:
    ComputeShader shader;
};
