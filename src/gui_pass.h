#pragma once

#include "render_pass.h"

class FPSTimer;
class GPUTimer;

class GuiPass : public RenderPass
{
public:
    GuiPass(const FPSTimer& fps, const GPUTimer& gpu);
    void execute(const RenderContext&, RenderTargets&) override;

private:
    const FPSTimer& fpsTimer;
    const GPUTimer& gpuTimer;
};
