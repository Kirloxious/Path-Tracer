#pragma once

#include <vector>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "core/window.h"

class Camera;
class FPSTimer;
class GPUTimer;
class PassTimings;
struct RenderSettings;
struct Scene;
struct SceneEntry;

namespace Gui {

struct SceneSwitchState
{
    int current = 0;
    int requested = -1; ///< -1 when nothing is pending.
};

void init(Window& window);

/// Call before the GL context is destroyed.
void shutdown();

/// Must precede any draw* call.
void beginFrame();

void endFrame();

void drawPerformance(const FPSTimer& fps, const GPUTimer& gpu);

void drawSceneSwitcher(const std::vector<SceneEntry>& entries, SceneSwitchState& state);

void drawScene(const Scene& scene);

void drawCamera(const Camera& camera);

void drawSettings(RenderSettings& settings);

void drawPassTimings(const PassTimings& passTimings);

void drawStats(const FPSTimer& fps, const GPUTimer& gpu, const PassTimings& passTimings, const Scene& scene, const Camera& camera,
               const std::vector<SceneEntry>& entries, SceneSwitchState& sceneSwitch, RenderSettings& settings);
} // namespace Gui
