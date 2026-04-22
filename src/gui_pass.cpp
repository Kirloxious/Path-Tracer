#include "gui_pass.h"

#include "gui.h"
#include "timer.h"

GuiPass::GuiPass(const FPSTimer& fps, const GPUTimer& gpu) : fpsTimer(fps), gpuTimer(gpu) {}

void GuiPass::execute(const RenderContext& ctx, RenderTargets&) {
    Gui::drawStats(fpsTimer, gpuTimer, ctx.scene, ctx.camera);
}
