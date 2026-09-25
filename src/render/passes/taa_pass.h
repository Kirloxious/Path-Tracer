#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

class TaaPass : public RenderPass
{
public:
    TaaPass();

    bool             reloadIfChanged() override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "TAA"; }

private:
    ComputeShader shader;
};
