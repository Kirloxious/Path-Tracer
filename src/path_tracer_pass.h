#pragma once

#include "render_pass.h"
#include "compute_shader.h"

class PathTracerPass : public RenderPass
{
public:
    explicit PathTracerPass(const std::filesystem::path&);
    void uploadUniforms(const RenderContext&) override;
    bool reloadIfChanged(RenderContext&) override;
    void execute(RenderContext&, RenderTargets&) override;

private:
    ComputeShader shader;
};
