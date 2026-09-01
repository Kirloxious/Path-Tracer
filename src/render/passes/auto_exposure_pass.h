#pragma once

/**
 * @file auto_exposure_pass.h
 * @brief Histogram-based automatic exposure, feeding the tonemap pass.
 */

#include "gpu/buffer.h"
#include "gpu/compute_shader.h"
#include "render/render_pass.h"
#include "render/render_settings.h"

/**
 * @brief Two-step luminance-based auto exposure.
 *
 *   1. `luminance_histogram.comp` builds a 256-bin log-luminance histogram over the HDR image.
 *   2. `auto_exposure.comp` reduces it and EMA-smooths the resulting exposure.
 *
 * The result lives in a persistent ExposureBuffer that TonemapPass reads, so the two passes
 * never exchange data on the CPU.
 *
 * When `settings.autoExposureEnabled` is false the SSBO is overwritten each frame with
 * `settings.exposure`, so TonemapPass doesn't have to branch.
 */
class AutoExposurePass : public RenderPass
{
public:
    /**
     * @brief Loads both kernels and allocates the exposure and histogram SSBOs.
     * @param width    Framebuffer width in pixels.
     * @param height   Framebuffer height in pixels.
     * @param settings Shared settings block; borrowed by reference and read every frame, so it
     *                 must outlive this pass.
     */
    AutoExposurePass(int width, int height, const RenderSettings& settings);

    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "AutoExpose"; }

private:
    int                   width;
    int                   height;
    const RenderSettings& settings;

    ComputeShader histogramShader;
    ComputeShader reduceShader;

    /// ExposureBuffer at binding 30 — first float is the current exposure; the rest is std430
    /// padding for the vec4-aligned struct.
    Buffer exposureSSBO;
    /// 256 uint bins at binding 31 — cleared inside the reduce shader.
    Buffer histogramSSBO;

    /// false until the first execute(), which writes the measured exposure directly rather than
    /// easing into it — otherwise every scene load fades in from the previous exposure. Reset
    /// on scene load and whenever auto-exposure is disabled, so the EMA re-seeds on re-enable.
    bool primed = false;
};
