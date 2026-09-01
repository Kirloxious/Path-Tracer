#pragma once

/**
 * @file gui_pass.h
 * @brief ImGui overlay pass; must be registered last.
 */

#include <vector>

#include "render/gui.h"
#include "render/render_pass.h"
#include "scene/scene.h"

class FPSTimer;
class GPUTimer;
class PassTimings;
struct RenderSettings;

/**
 * @brief Draws the ImGui overlay on top of the resolved image.
 *
 * Holds only borrowed references — every one of its constructor arguments is owned by
 * Application and must outlive the pass. Must be the last pass registered, so the overlay
 * is not overwritten by a later stage.
 */
class GuiPass : public RenderPass
{
public:
    /**
     * @brief Binds the pass to the state it displays and mutates.
     * @param fps          CPU frame timer, shown in the performance panel.
     * @param gpu          Whole-frame GPU timer, shown alongside it.
     * @param passTimings  Per-pass GPU timings for the breakdown panel.
     * @param sceneEntries Scene registry backing the selector.
     * @param sceneSwitch  Selector state; the pass writes `requested` when the user picks a scene.
     * @param settings     Render settings, mutated in place by the GUI widgets.
     */
    GuiPass(const FPSTimer& fps, const GPUTimer& gpu, const PassTimings& passTimings, const std::vector<SceneEntry>& sceneEntries,
            Gui::SceneSwitchState& sceneSwitch, RenderSettings& settings);

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
