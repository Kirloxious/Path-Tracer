#pragma once

#include "gpu/buffer.h"
#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/// Writes the exposure SSBO TonemapPass reads; when disabled it writes `settings.exposure` instead,
/// so tonemap never branches.
class AutoExposurePass : public RenderPass
{
public:
    explicit AutoExposurePass(float initialExposure);

    bool             reloadIfChanged() override;
    void             resize(int width, int height) override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "AutoExpose"; }

private:
    ComputeShader histogramShader;
    ComputeShader reduceShader;

    /// std430 vec4: first float is the exposure, the rest padding.
    Buffer exposureSSBO;
    /// Cleared inside the reduce shader.
    Buffer histogramSSBO;

    /// False until the first execute(), which snaps to the measured exposure instead of fading in
    /// from the previous scene's.
    bool primed = false;
};
