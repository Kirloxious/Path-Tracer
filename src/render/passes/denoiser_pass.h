#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

class DenoiserPass : public RenderPass
{
public:
    explicit DenoiserPass(const std::filesystem::path&);
    void        uploadUniforms(const Scene&, const Camera&) override;
    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Denoiser"; }

private:
    ComputeShader shader;
};
