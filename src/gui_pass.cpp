#include "gui_pass.h"

#include "gui.h"
#include "timer.h"

GuiPass::GuiPass(const FPSTimer& fps, const GPUTimer& gpu, const std::vector<SceneEntry>& sceneEntries, Gui::SceneSwitchState& sceneSwitch)
    : fpsTimer(fps), gpuTimer(gpu), sceneEntries(sceneEntries), sceneSwitch(sceneSwitch) {}

void GuiPass::execute(const RenderContext& ctx, RenderTargets&) {
    Gui::drawStats(fpsTimer, gpuTimer, ctx.scene, ctx.camera, sceneEntries, sceneSwitch);
}
