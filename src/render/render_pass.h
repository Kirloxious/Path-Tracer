#pragma once

/**
 * @file render_pass.h
 * @brief The RenderPass contract and the per-frame context handed to every pass.
 */

#include "render/render_targets.h"
#include <scene/scene.h>

/**
 * @brief Per-frame state passed by reference to every RenderPass call.
 *
 * Holds only what a pass cannot own itself. Intermediate images live in RenderTargets, which
 * is passed alongside this to execute().
 */
struct RenderContext
{
    const Scene&  scene;
    const Camera& camera;
    /// Frames accumulated since the last reset; 1 on the first frame of a new accumulation, 0 on
    /// a frame whose shaders were just reloaded.
    int frameIndex;
    /// Advances every frame. Seeds the PCG streams (reservoir acceptance), which want a fresh
    /// sequence per frame. NOT for the low-discrepancy sampler — see runSeed.
    uint32_t timeSeed;
    /// Seconds since the last frame — used by EMA-style passes (auto-exposure).
    float dt;
    /// Constant for one accumulation; changes only when `frameIndex` resets. The low-discrepancy
    /// sampler's per-pixel seed folds this in and nothing else: its stratification is *across
    /// frames*, so a seed that changed mid-accumulation would reduce the sequence to white
    /// noise. `frameIndex` is its sample index.
    uint32_t runSeed;
    /// Frames since temporal history (TAA, ReSTIR reservoirs) was last invalidated by a resize,
    /// scene switch or shader reload. Unlike `frameIndex` it survives camera motion, which
    /// reprojection exists to follow.
    int historyFrames;
};

/**
 * @brief Interface every rendering stage implements; the only contract Renderer knows about.
 *
 * Passes are registered with Renderer::addRenderPass() and run in registration order. The
 * order is load-bearing: Raster → ReSTIR → PathTracer → Denoiser → Bloom → AutoExposure →
 * Tonemap → TAA → AOV → Gui, with GuiPass necessarily last.
 *
 * Non-copyable — passes own GL resources.
 */
class RenderPass
{
public:
    virtual ~RenderPass() = default;

    /**
     * @brief Called after Renderer::loadScene() has uploaded a new scene.
     *
     * Scene-wide shader inputs already live in the SceneConstants UBO; this hook is for passes
     * that own scene-derived GPU data of their own (raster geometry) or history that a new
     * scene invalidates (ReSTIR reservoirs).
     */
    virtual void onSceneLoaded(const Scene&) {}

    /**
     * @brief Rebuilds this pass's shaders if their sources changed on disk.
     *
     * Called once per frame. Application resets accumulation when any pass returns true,
     * since the new program may have changed the meaning of the accumulated samples. Nothing
     * needs re-uploading: frame and scene values come from UBOs, and pass-private uniforms
     * are set in execute().
     *
     * @return true if a shader was successfully rebuilt this frame.
     */
    virtual bool reloadIfChanged() { return false; }

    /**
     * @brief Called when the framebuffer size changes.
     *
     * Takes the new framebuffer width and height in pixels. Default is a no-op; only passes
     * that own size-dependent GPU buffers need to override it — the current size is always
     * available from RenderTargets.
     */
    virtual void resize(int /*width*/, int /*height*/) {}

    /**
     * @brief Records this pass's GPU work for the current frame.
     * @param ctx     Per-frame state.
     * @param targets Shared intermediate images; passes both read and write these in place.
     */
    virtual void execute(const RenderContext& ctx, RenderTargets& targets) = 0;

    /**
     * @brief Short display name for the per-pass GPU timer panel.
     *
     * The default placeholder means adding a new pass doesn't force a rebuild of the panel.
     * Must return a static string — PassTimings stores it by pointer.
     *
     * @return The pass's label.
     */
    virtual const char* name() const { return "Pass"; }

    RenderPass() = default;
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
};
