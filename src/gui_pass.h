#pragma once

#include <vector>

#include "gui.h"
#include "render_pass.h"
#include "scene.h"

class FPSTimer;
class GPUTimer;

class GuiPass : public RenderPass
{
public:
    GuiPass(const FPSTimer& fps, const GPUTimer& gpu, const std::vector<SceneEntry>& sceneEntries, Gui::SceneSwitchState& sceneSwitch);
    void execute(const RenderContext&, RenderTargets&) override;

private:
    const FPSTimer&                fpsTimer;
    const GPUTimer&                gpuTimer;
    const std::vector<SceneEntry>& sceneEntries;
    Gui::SceneSwitchState&         sceneSwitch;
};
