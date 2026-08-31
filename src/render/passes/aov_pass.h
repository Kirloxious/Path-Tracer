#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"
#include "render/render_settings.h"

// Runs after the denoiser and, when `settings.aovMode != None`, overwrites the
// display texture with a debug visualization (world normal / linear depth /
// albedo / material ID / BVH-traversal cost heatmap / per-pixel variance).
//
// When the mode is None the pass early-exits without dispatching, so the
// tonemapped denoised image reaches the swap chain untouched.
class AovPass : public RenderPass
{
public:
    AovPass(int width, int height, const RenderSettings& settings);

    void        uploadUniforms(const Scene&, const Camera&) override;
    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "AOV"; }

private:
    int                   width;
    int                   height;
    ComputeShader         shader;
    const RenderSettings& settings;
};
