#pragma once

#include "render_targets.h"
#include <scene.h>

struct RenderContext
{
    const Scene&  scene;
    const Camera& camera;
    int           frameIndex;
    uint32_t      timeSeed;

public:
    void resetFrameIndex() { frameIndex = 0; }
};

class RenderPass
{
public:
    virtual ~RenderPass() = default;

    virtual void uploadUniforms(const RenderContext&) {}

    virtual bool reloadIfChanged(RenderContext&) { return false; }

    virtual void execute(RenderContext&, RenderTargets&) = 0;

    RenderPass() = default;
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
};
