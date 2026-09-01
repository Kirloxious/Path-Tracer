#pragma once

/**
 * @file renderer.h
 * @brief Owns the scene GPU buffers, the render targets and the ordered pass list.
 */

#include <memory>
#include <vector>

#include "gpu/buffer.h"
#include "gpu/env_map.h"
#include "render/render_pass.h"
#include "gpu/texture.h"
#include "gpu/timer.h"

/**
 * @brief Drives the pass chain and owns everything the passes share.
 *
 * Holds the scene-wide SSBOs/UBO (light groups, materials, camera, BVH nodes, triangles,
 * vertices), the environment map, the RenderTargets, and the pass list in execution order.
 * Passes never touch these buffers directly — they are bound once at their binding points
 * and read by the shaders.
 */
class Renderer
{
public:
    /**
     * @brief Allocates the render targets at the initial framebuffer size.
     * @param w Width in pixels.
     * @param h Height in pixels.
     */
    Renderer(int w, int h);

    /**
     * @brief (Re)uploads all scene buffers and re-fires uploadUniforms() on every pass.
     *
     * Used both at startup and when the GUI requests a scene switch. An unlit scene is legal:
     * the light-groups SSBO is simply not uploaded. The envmap is rebuilt from
     * `scene.envMapPath`, or cleared to an invalid EnvMap when the path is empty.
     *
     * @param scene  Scene to upload. Must already have had World::create() run on it.
     * @param camera Camera whose CameraData seeds the UBO.
     */
    void loadScene(const Scene& scene, const Camera& camera);

    /**
     * @brief Reallocates the render targets and forwards the new size to every pass.
     *
     * Non-positive dimensions are ignored (a minimised window reports 0x0). All accumulated
     * image data is discarded, so the caller must reset `frameIndex`.
     *
     * @param w New width in pixels.
     * @param h New height in pixels.
     */
    void resize(int w, int h);

    /**
     * @brief Uploads the camera's current CameraData into the UBO at binding 2.
     * @param cam Camera to read `data` from. Call after Camera::update() and applyJitter().
     */
    void updateCameraUbo(const Camera& cam);

    /**
     * @brief Executes every registered pass in order, bracketing each with a GPU timer.
     * @param ctx Per-frame state forwarded to each pass.
     */
    void render(RenderContext& ctx);

    /**
     * @brief Gives every pass a chance to hot-reload its shaders.
     * @param ctx Per-frame state forwarded to each pass.
     * @return true if any pass rebuilt a shader — the caller should then reset `frameIndex`.
     */
    bool reloadShadersIfChanged(RenderContext& ctx);

    /**
     * @brief Appends a pass to the chain and registers it with the per-pass timer panel.
     *
     * Registration order is execution order, and the order is load-bearing (see RenderPass).
     *
     * @param pass Pass to take ownership of.
     */
    void addRenderPass(std::unique_ptr<RenderPass> pass);

    /**
     * @brief Blits the final `display` target to the default framebuffer.
     * @param width  Destination width in pixels.
     * @param height Destination height in pixels.
     */
    void blitToSwapChain(int width, int height);

    /**
     * @brief Blits a G-buffer attachment to the default framebuffer instead of the final image.
     *
     * Backs the F1/F2 debug views in Application::run().
     *
     * @param attachmentIndex GBuffer::ATTACH_NORMAL (the only colour attachment).
     * @param width           Destination width in pixels.
     * @param height          Destination height in pixels.
     */
    void blitGBufferAttachmentToSwapChain(int attachmentIndex, int width, int height);

    /// @return The per-pass GPU timings, for the GUI panel.
    const PassTimings& getPassTimings() const { return passTimings; }

private:
    RenderTargets targets;
    Buffer        lightGroupsSSBO, matsSSBO, camUBO, bvhNodesSSBO, trianglesSSBO, verticesSSBO, triRefsSSBO;
    EnvMap        envMap;
    PassTimings   passTimings;

    std::vector<std::unique_ptr<RenderPass>> passes;
};
