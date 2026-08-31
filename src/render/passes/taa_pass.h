#pragma once

#include "gpu/compute_shader.h"
#include "render/render_pass.h"

// Temporal anti-aliasing. Reads targets.display (this frame's tonemapped image) and
// targets.taa_history (last frame's TAA output, sampled bilinearly), reprojects via
// the camera UBO's un-jittered prev_view_proj_matrix using the primary hit's world
// position from the gbuffer, applies a 3×3 RGB neighborhood clamp to suppress
// ghosting, and blends. Result is written into targets.taa_output, then copied back
// into targets.display so downstream passes (AOV, swap-chain blit) see the resolved
// image. taa_output and taa_history are swapped so next frame's history is this
// frame's result.
class TaaPass : public RenderPass
{
public:
    TaaPass(int width, int height);

    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "TAA"; }

private:
    int           width;
    int           height;
    ComputeShader shader;
};
