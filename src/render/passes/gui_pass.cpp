#include "render/passes/gui_pass.h"

#include "render/gui.h"
#include "render/render_settings.h"
#include "gpu/timer.h"

GuiPass::GuiPass(const FPSTimer& fps, const GPUTimer& gpu, const PassTimings& passTimings, const std::vector<SceneEntry>& sceneEntries,
                 Gui::SceneSwitchState& sceneSwitch, RenderSettings& settings)
    : fpsTimer(fps), gpuTimer(gpu), passTimings(passTimings), sceneEntries(sceneEntries), sceneSwitch(sceneSwitch), settings(settings) {}

void GuiPass::execute(const RenderContext& ctx, RenderTargets&) {
    Gui::drawStats(fpsTimer, gpuTimer, passTimings, ctx.scene, ctx.camera, sceneEntries, sceneSwitch, settings);
}
