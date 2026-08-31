#pragma once

#include <vector>

#include "render/gui.h"
#include "render/render_pass.h"
#include "scene/scene.h"

class FPSTimer;
class GPUTimer;
class PassTimings;
struct RenderSettings;

class GuiPass : public RenderPass
{
public:
    GuiPass(const FPSTimer&                fps,
            const GPUTimer&                gpu,
            const PassTimings&             passTimings,
            const std::vector<SceneEntry>& sceneEntries,
            Gui::SceneSwitchState&         sceneSwitch,
            RenderSettings&                settings);
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Gui"; }

private:
    const FPSTimer&                fpsTimer;
    const GPUTimer&                gpuTimer;
    const PassTimings&             passTimings;
    const std::vector<SceneEntry>& sceneEntries;
    Gui::SceneSwitchState&         sceneSwitch;
    RenderSettings&                settings;
};
