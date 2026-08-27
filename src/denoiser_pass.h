#pragma once

#include "compute_shader.h"
#include "render_pass.h"
#include "render_settings.h"

class DenoiserPass : public RenderPass
{
public:
    DenoiserPass(const std::filesystem::path&, const RenderSettings& settings);
    void uploadUniforms(const Scene&, const Camera&) override;
    bool reloadIfChanged(const RenderContext&) override;
    void execute(const RenderContext&, RenderTargets&) override;

private:
    ComputeShader         shader;
    const RenderSettings& settings;
};
