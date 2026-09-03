#pragma once

/**
 * @file gui.h
 * @brief ImGui overlay: performance panels, scene switcher and runtime settings.
 */

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

/**
 * @brief Stateless ImGui drawing helpers, called from GuiPass::execute().
 *
 * Every draw* function assumes beginFrame() has already run this frame and that endFrame()
 * will run after. The only state the GUI itself carries between frames is SceneSwitchState,
 * which the caller owns.
 */
namespace Gui {

/**
 * @brief The scene selector's state, owned by Application and mutated by drawSceneSwitcher().
 *
 * Application polls `requested` each frame; a non-negative value means the user picked a new
 * scene, which is applied (and the field reset to -1) at a safe point in the frame.
 */
struct SceneSwitchState
{
    int current = 0;    ///< Index into the scene registry currently loaded.
    int requested = -1; ///< Index the user picked, or -1 when nothing is pending.
};

/**
 * @brief Creates the ImGui context and installs the GLFW + OpenGL3 backends.
 * @param window The window whose GL context and event callbacks ImGui hooks into.
 */
void init(Window& window);

/// Tears down the ImGui backends and context. Call before the GL context is destroyed.
void shutdown();

/// Starts an ImGui frame. Must precede any draw* call.
void beginFrame();

/// Ends the ImGui frame and issues its draw commands to the current framebuffer.
void endFrame();

/**
 * @brief Draws the FPS / frame-time / GPU-time readouts and their history plots.
 * @param fps CPU-side frame timer.
 * @param gpu Whole-frame GPU timer.
 */
void drawPerformance(const FPSTimer& fps, const GPUTimer& gpu);

/**
 * @brief Draws the scene selector combo.
 * @param entries All registered scenes, in menu order.
 * @param state   Selector state; `requested` is set when the user picks a different scene.
 */
void drawSceneSwitcher(const std::vector<SceneEntry>& entries, SceneSwitchState& state);

/**
 * @brief Draws read-only scene statistics and the collapsible object list.
 * @param scene Currently loaded scene.
 */
void drawScene(const Scene& scene);

/**
 * @brief Draws read-only camera position, forward vector, yaw/pitch and field of view.
 * @param camera Current camera.
 */
void drawCamera(const Camera& camera);

/**
 * @brief Draws the editable render settings (exposure, bloom, auto-exposure, AOV mode).
 * @param settings Settings block, mutated in place by the widgets.
 */
void drawSettings(RenderSettings& settings);

/**
 * @brief Draws the per-pass GPU timing breakdown.
 * @param passTimings Timings collected by Renderer during the previous frames.
 */
void drawPassTimings(const PassTimings& passTimings);

/**
 * @brief Draws the whole overlay — every panel above in one window.
 * @param fps          CPU-side frame timer.
 * @param gpu          Whole-frame GPU timer.
 * @param passTimings  Per-pass GPU timings.
 * @param scene        Currently loaded scene.
 * @param camera       Current camera.
 * @param entries      All registered scenes, in menu order.
 * @param sceneSwitch  Selector state; mutated when the user picks a scene.
 * @param settings     Render settings, mutated in place by the widgets.
 */
void drawStats(const FPSTimer& fps, const GPUTimer& gpu, const PassTimings& passTimings, const Scene& scene, const Camera& camera,
               const std::vector<SceneEntry>& entries, SceneSwitchState& sceneSwitch, RenderSettings& settings);
}; // namespace Gui
