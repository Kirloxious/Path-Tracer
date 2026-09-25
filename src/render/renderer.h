#pragma once

#include <memory>
#include <vector>

#include "gpu/buffer.h"
#include "gpu/env_map.h"
#include "render/render_pass.h"
#include "gpu/texture.h"
#include "gpu/timer.h"

class Renderer
{
public:
    Renderer(int w, int h);

    /// Throws if the envmap fails to load or World::create() was never run; nothing is replaced
    /// by then, so the previous scene stays usable.
    void loadScene(const Scene& scene, const Camera& camera);

    /// Ignores non-positive sizes (a minimised window reports 0x0). The caller must reset `frameIndex`.
    void resize(int w, int h);

    /// Call after Camera::update() and applyJitter().
    void updateCameraUbo(const Camera& cam);

    void render(const RenderContext& ctx);

    /// @return true if any pass rebuilt a shader; the caller should then reset `frameIndex`.
    bool reloadShadersIfChanged();

    /// Registration order is execution order, and the order is load-bearing.
    void addRenderPass(std::unique_ptr<RenderPass> pass);

    void blitToSwapChain(int width, int height);

    void blitGBufferAttachmentToSwapChain(int attachmentIndex, int width, int height);

    const PassTimings& getPassTimings() const { return passTimings; }

private:
    void bindSharedResources() const;

    RenderTargets targets;
    Buffer        lightGroupsSSBO, matsSSBO, bvhNodesSSBO, trianglesSSBO, verticesSSBO, triRefsSSBO, envSamplingSSBO;
    Buffer        camUBO, frameUBO, sceneUBO;
    EnvMap        envMap;
    PassTimings   passTimings;

    std::vector<std::unique_ptr<RenderPass>> passes;
};
