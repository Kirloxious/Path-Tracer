#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

// Reads targets.hdr, applies exposure (from the persistent ExposureBuffer SSBO
// written by AutoExposurePass) followed by ACES + sRGB, writes targets.display.
class TonemapPass : public RenderPass
{
public:
    TonemapPass(int width, int height);

    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Tonemap"; }

private:
    int           width;
    int           height;
    ComputeShader shader;
};
