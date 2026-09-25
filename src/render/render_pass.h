#pragma once

#include <string_view>

#include "render/render_settings.h"
#include "render/render_targets.h"
#include "scene/scene.h"

struct RenderContext
{
    const Scene&  scene;
    const Camera& camera;
    /// Mutating these never resets accumulation.
    const RenderSettings& settings;
    /// 1 on the first frame of a new accumulation, 0 on a frame whose shaders were just reloaded.
    int frameIndex;
    /// Fresh every frame, for the PCG streams. NOT for the low-discrepancy sampler; see runSeed.
    uint32_t timeSeed;
    float    dt;
    /// Constant for one accumulation: the sampler stratifies across frames, so a seed changing
    /// mid-accumulation would reduce it to white noise.
    uint32_t runSeed;
    /// Frames since temporal history was invalidated (resize, scene switch, reload). Unlike
    /// `frameIndex` it survives camera motion.
    int historyFrames;
};

/// Passes run in registration order, which is load-bearing. A pass binds only resources it owns;
/// Renderer binds everything scene- and frame-wide before the first pass.
class RenderPass
{
public:
    virtual ~RenderPass() = default;

    /// For passes owning scene-derived GPU data or history a new scene invalidates.
    virtual void onSceneLoaded(const Scene&) {}

    /// @return true if a shader was rebuilt; Application then resets accumulation.
    virtual bool reloadIfChanged() { return false; }

    virtual void resize(int /*width*/, int /*height*/) {}

    virtual void execute(const RenderContext& ctx, RenderTargets& targets) = 0;

    virtual std::string_view name() const = 0;

    RenderPass() = default;
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
};
