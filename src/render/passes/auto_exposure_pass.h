#pragma once

#include "gpu/buffer.h"
#include "gpu/compute_shader.h"
#include "render/render_pass.h"
#include "render/render_settings.h"

// Two-step luminance-based auto exposure:
//   1. luminance_histogram.comp builds a 256-bin log-luminance histogram.
//   2. auto_exposure.comp reduces it and EMA-smooths the resulting exposure.
// The result lives in a persistent ExposureBuffer that TonemapPass reads.
//
// When settings.autoExposureEnabled is false, the SSBO is overwritten each
// frame with settings.exposure so TonemapPass doesn't have to branch.
class AutoExposurePass : public RenderPass
{
public:
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

    // ExposureBuffer at binding 30 — first float is the current exposure; the
    // rest is std430 padding for the vec4-aligned struct.
    Buffer exposureSSBO;
    // 256 uint bins at binding 31 — cleared inside the reduce shader.
    Buffer histogramSSBO;

    bool primed = false; // first execute() writes exposure without EMA
};
