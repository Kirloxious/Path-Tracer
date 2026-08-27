#include "gui_pass.h"

#include "gui.h"
#include "render_settings.h"
#include "timer.h"

GuiPass::GuiPass(const FPSTimer& fps, const GPUTimer& gpu, const std::vector<SceneEntry>& sceneEntries, Gui::SceneSwitchState& sceneSwitch,
                 RenderSettings& settings)
    : fpsTimer(fps), gpuTimer(gpu), sceneEntries(sceneEntries), sceneSwitch(sceneSwitch), settings(settings) {}

void GuiPass::execute(const RenderContext& ctx, RenderTargets&) {
    Gui::drawStats(fpsTimer, gpuTimer, ctx.scene, ctx.camera, sceneEntries, sceneSwitch, settings);
}
