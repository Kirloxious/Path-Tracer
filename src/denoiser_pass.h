#pragma once

#include "render_pass.h"
#include "compute_shader.h"

class DenoiserPass : public RenderPass
{
public:
    explicit DenoiserPass(const std::filesystem::path&);
    void uploadUniforms(const RenderContext&) override;
    bool reloadIfChanged(RenderContext&) override;
    void execute(RenderContext&, RenderTargets&) override;

private:
    ComputeShader shader;
};
