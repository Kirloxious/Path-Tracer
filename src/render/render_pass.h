#pragma once

#include "render/render_targets.h"
#include <scene/scene.h>

struct RenderContext
{
    const Scene&  scene;
    const Camera& camera;
    int           frameIndex;
    uint32_t      timeSeed;
    float         dt; // seconds since last frame — used by EMA-style passes (auto-exposure)

public:
    void resetFrameIndex() { frameIndex = 0; }
};

class RenderPass
{
public:
    virtual ~RenderPass() = default;

    // Called on scene load and after a successful shader reload. Only the
    // scene and camera are available — nothing per-frame (frameIndex, timeSeed).
    // Per-frame uniforms belong in execute() where they can read a real ctx.
    virtual void uploadUniforms(const Scene&, const Camera&) {}

    virtual bool reloadIfChanged(const RenderContext&) { return false; }

    // Called when the framebuffer size changes. Default no-op; passes that cache
    // dimensions or own per-pixel GPU buffers must override to reallocate.
    virtual void resize(int /*width*/, int /*height*/) {}

    virtual void execute(const RenderContext&, RenderTargets&) = 0;

    // Short display name for per-pass GPU timers. Default is a placeholder so
    // adding a new pass doesn't force a rebuild of the timer panel.
    virtual const char* name() const { return "Pass"; }

    RenderPass() = default;
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
};
