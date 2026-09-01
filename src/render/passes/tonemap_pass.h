#pragma once

/**
 * @file tonemap_pass.h
 * @brief HDR to LDR conversion: exposure, ACES and sRGB encoding.
 */

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/**
 * @brief Converts `targets.hdr` into the LDR `targets.display` image.
 *
 * Applies exposure — read from the persistent ExposureBuffer SSBO that AutoExposurePass
 * writes, so this pass never has to know whether exposure is automatic or manual — followed by
 * the ACES filmic curve and linear-to-sRGB encoding.
 */
class TonemapPass : public RenderPass
{
public:
    /**
     * @brief Loads `tonemap.comp` and caches the dispatch dimensions.
     * @param width  Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     */
    TonemapPass(int width, int height);

    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Tonemap"; }

private:
    int           width;
    int           height;
    ComputeShader shader;
};
