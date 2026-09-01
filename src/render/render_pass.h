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
    /// Frames accumulated since the last reset. 0 means "first frame of a new accumulation",
    /// which passes use to skip temporal reuse and to prime EMA state.
    int frameIndex;
    /// Per-run random seed, mixed into the GPU RNG so different runs don't share frame-1 noise.
    uint32_t timeSeed;
    /// Seconds since the last frame — used by EMA-style passes (auto-exposure).
    float dt;
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
     * @brief Uploads scene-derived uniforms. Called on scene load and after a successful reload.
     *
     * Takes the newly loaded Scene and the Camera it was loaded with. Nothing per-frame
     * (frameIndex, timeSeed) is available here — those uniforms belong in execute(), where a
     * real RenderContext is in hand. Parameters are unnamed in the default implementation so
     * passes with no scene-derived uniforms need not override it.
     */
    virtual void uploadUniforms(const Scene&, const Camera&) {}

    /**
     * @brief Rebuilds this pass's shaders if their sources changed on disk.
     *
     * Called once per frame. A pass that returns true is expected to have already reuploaded
     * its scene-derived uniforms; Application additionally resets `frameIndex`, since the new
     * program may have changed the meaning of the accumulated samples. Takes the current
     * RenderContext, which passes need in order to reupload per-frame uniforms after a rebuild.
     *
     * @return true if a shader was successfully rebuilt this frame.
     */
    virtual bool reloadIfChanged(const RenderContext&) { return false; }

    /**
     * @brief Called when the framebuffer size changes.
     *
     * Takes the new framebuffer width and height in pixels. Default is a no-op; passes that
     * cache dimensions or own per-pixel GPU buffers must override to reallocate.
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
