#pragma once

#include <vector>

#include "gpu/compute_shader.h"
#include "gpu/texture.h"
#include "render/render_pass.h"
#include "render/render_settings.h"

// COD Advances / Jimenez bloom: partial-Karis 13-tap downsample chain, then a
// 3x3 tent upsample chain additively blended into progressively larger mips
// and finally into the HDR image. Enabled/disabled via settings.bloomEnabled.
class BloomPass : public RenderPass
{
public:
    BloomPass(int width, int height, const RenderSettings& settings);

    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Bloom"; }

private:
    void buildMips(int w, int h);

    const RenderSettings& settings;

    ComputeShader downsampleShader;
    ComputeShader upsampleShader;

    // Progressive halving of the source resolution. mips[0] is w/2 x h/2.
    std::vector<Texture> mips;
    std::vector<int>     mipWidths;
    std::vector<int>     mipHeights;
};
