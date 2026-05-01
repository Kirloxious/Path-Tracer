#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "buffer.h"
#include "compute_shader.h"
#include "render_pass.h"

// CPU-side mirror of shader/common/restir_common.glsl `Reservoir`.
// Layout/size must match the std430 declaration exactly — we never upload
// reservoir data from CPU, but the static_assert documents the invariant.
struct alignas(16) ReservoirData
{
    uint32_t  light_tri_idx;
    float     M;
    glm::vec2 bary;
    float     w_sum;
    float     W;
    float     target_pdf;
    float     _pad;
};
static_assert(sizeof(ReservoirData) == 32, "Reservoir size must match std430 layout");

// ReSTIR DI pass.
//
// Phase 1: only the initial-candidate kernel is dispatched. For each pixel,
// the gbuffer hit is the surface; M candidate lights are drawn from the
// existing area-CDF NEE sampler, ranked by RIS using a luminance-of-
// (f * Le * G) target, and the chosen sample is shadow-tested. The result
// is one Reservoir per pixel in the current-frame SSBO at binding 18.
//
// Phase 2 will add temporal reuse, Phase 3 spatial reuse. Phase 4 hooks
// the final reservoir into shade_lambertian for primary-hit direct lighting.
class RestirPass : public RenderPass
{
public:
    RestirPass(int width, int height);

    void uploadUniforms(const RenderContext&) override;
    bool reloadIfChanged(const RenderContext&) override;
    void execute(const RenderContext&, RenderTargets&) override;

private:
    void clearReservoirBuffers();

    int width;
    int height;
    int numPixels;
    int numWorkGroupsX;
    int numWorkGroupsY;

    ComputeShader initial;
    ComputeShader temporal;
    ComputeShader spatial;

    // Two reservoir buffers ping-ponged each frame: one bound at binding 18
    // ("current", written this frame), the other at binding 19 ("prev",
    // read-only this frame). Roles swap at the top of each execute().
    Buffer reservoirsA;
    Buffer reservoirsB;
    bool   useAAsCurrent = true;
};
