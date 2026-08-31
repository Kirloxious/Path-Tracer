#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"
#include "render/render_settings.h"

class DenoiserPass : public RenderPass
{
public:
    DenoiserPass(const std::filesystem::path&, const RenderSettings& settings);
    void        uploadUniforms(const Scene&, const Camera&) override;
    bool        reloadIfChanged(const RenderContext&) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Denoiser"; }

private:
    ComputeShader         shader;
    const RenderSettings& settings;
};
