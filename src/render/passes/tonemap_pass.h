#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

class TonemapPass : public RenderPass
{
public:
    TonemapPass();

    bool             reloadIfChanged() override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "Tonemap"; }

private:
    ComputeShader shader;
};
