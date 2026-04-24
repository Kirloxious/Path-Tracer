#pragma once

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "window.h"

class Camera;
class FPSTimer;
class GPUTimer;
struct Scene;

namespace Gui {
void init(Window&);
void shutdown();
void beginFrame();
void endFrame();

void drawPerformance(const FPSTimer&, const GPUTimer&);
void drawScene(const Scene&);
void drawCamera(const Camera&);
void drawStats(const FPSTimer&, const GPUTimer&, const Scene&, const Camera&);
}; // namespace Gui
