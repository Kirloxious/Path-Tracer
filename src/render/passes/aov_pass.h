#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/// Overwrites `display` with a debug view when `settings.aovMode != None`; otherwise dispatches nothing.
class AovPass : public RenderPass
{
public:
    AovPass();

    bool             reloadIfChanged() override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "AOV"; }

private:
    ComputeShader shader;
};
