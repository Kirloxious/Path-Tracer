#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/// Each pass filters a variance estimate alongside the colour and hands it on in alpha, so wide
/// taps size their edge-stop for the noise still present.
class DenoiserPass : public RenderPass
{
public:
    explicit DenoiserPass(const std::filesystem::path& shaderPath);

    bool             reloadIfChanged() override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "Denoiser"; }

private:
    ComputeShader shader;
};
