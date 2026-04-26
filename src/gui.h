#pragma once

#include <vector>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "window.h"

class Camera;
class FPSTimer;
class GPUTimer;
struct Scene;
struct SceneEntry;

namespace Gui {

struct SceneSwitchState
{
    int current = 0;
    int requested = -1;
};

void init(Window&);
void shutdown();
void beginFrame();
void endFrame();

void drawPerformance(const FPSTimer&, const GPUTimer&);
void drawSceneSwitcher(const std::vector<SceneEntry>&, SceneSwitchState&);
void drawScene(const Scene&);
void drawCamera(const Camera&);
void drawStats(const FPSTimer&, const GPUTimer&, const Scene&, const Camera&, const std::vector<SceneEntry>&, SceneSwitchState&);
}; // namespace Gui
