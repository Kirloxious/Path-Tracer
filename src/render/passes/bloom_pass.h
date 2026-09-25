#pragma once

#include <vector>

#include "gpu/compute_shader.h"
#include "gpu/texture.h"
#include "render/render_pass.h"

/// COD Advanced Warfare / Jimenez bloom: 13-tap partial-Karis downsample chain, 3x3 tent upsample.
class BloomPass : public RenderPass
{
public:
    BloomPass(int width, int height);

    bool             reloadIfChanged() override;
    void             resize(int width, int height) override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "Bloom"; }

private:
    void buildMips(int w, int h);

    ComputeShader downsampleShader;
    ComputeShader upsampleShader;

    /// mips[0] is w/2 x h/2.
    std::vector<Texture> mips;
};
