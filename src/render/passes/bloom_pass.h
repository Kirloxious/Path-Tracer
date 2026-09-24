#pragma once

/**
 * @file bloom_pass.h
 * @brief Mip-chain bloom applied to the HDR image before tonemapping.
 */

#include <vector>

#include "gpu/compute_shader.h"
#include "gpu/texture.h"
#include "render/render_pass.h"

/**
 * @brief COD Advances / Jimenez bloom over a progressively halved mip chain.
 *
 * A partial-Karis 13-tap downsample chain builds the mips, then a 3x3 tent upsample chain
 * blends each mip additively into the next larger one and finally into `targets.hdr`.
 * Enabled and tuned entirely through RenderSettings; disabled means the pass does nothing.
 */
class BloomPass : public RenderPass
{
public:
    /**
     * @brief Loads both kernels and allocates the mip chain for the given size.
     * @param width    Framebuffer width in pixels.
     * @param height   Framebuffer height in pixels.
     */
    BloomPass(int width, int height);

    bool             reloadIfChanged() override;
    void             resize(int width, int height) override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "Bloom"; }

private:
    /**
     * @brief Reallocates the mip chain by repeatedly halving @p w x @p h.
     * @param w Source width in pixels.
     * @param h Source height in pixels.
     */
    void buildMips(int w, int h);

    ComputeShader downsampleShader;
    ComputeShader upsampleShader;

    /// Progressive halving of the source resolution. mips[0] is w/2 x h/2.
    std::vector<Texture> mips;
};
