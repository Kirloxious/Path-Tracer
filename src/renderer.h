#pragma once

#include <memory>
#include <vector>

#include "buffer.h"
#include "render_pass.h"
#include "texture.h"

class Renderer
{
public:
    Renderer(int w, int h);

    void     loadScene(const Scene& scene, const Camera& camera);
    void     resize(int w, int h);
    void     updateCameraUbo(const Camera&);
    Texture& render(RenderContext&);
    bool     reloadShadersIfChanged(RenderContext&);
    void     addRenderPass(std::unique_ptr<RenderPass> pass);
    void     blitToSwapChain(Texture&, int width, int height);
    void     blitGBufferAttachmentToSwapChain(int attachmentIndex, int width, int height);

private:
    RenderTargets targets;
    Buffer        spheresSSBO, matsSSBO, camUBO, bvhNodesSSBO, trianglesSSBO;

    std::vector<std::unique_ptr<RenderPass>> passes;
};
