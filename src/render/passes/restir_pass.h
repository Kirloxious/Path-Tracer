#pragma once

/**
 * @file restir_pass.h
 * @brief ReSTIR DI: reservoir resampling of direct lighting at primary surfaces.
 */

#include <cstdint>
#include <glm/glm.hpp>

#include "gpu/buffer.h"
#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/**
 * @brief CPU-side mirror of the `Reservoir` struct in `shader/common/restir_common.glsl`.
 *
 * Layout and size must match the std430 declaration exactly. Reservoir data is never uploaded
 * from the CPU — the static_assert exists purely to document and enforce the invariant.
 */
struct alignas(16) ReservoirData
{
    uint32_t  light_tri_idx; ///< Index of the selected emissive triangle.
    float     M;             ///< Effective sample count folded into this reservoir.
    glm::vec2 bary;          ///< Barycentric coordinates of the sample on that triangle.
    float     w_sum;         ///< Running sum of resampling weights.
    float     W;             ///< Unbiased contribution weight; 0 means occluded or invalid.
    float     target_pdf;    ///< Target function value for the selected sample.
    float     _pad;
};
static_assert(sizeof(ReservoirData) == 32, "Reservoir size must match std430 layout");

/**
 * @brief ReSTIR DI at primary surfaces, producing one reservoir per pixel.
 *
 * Three back-to-back compute stages, all operating on the G-buffer hit as the shading surface:
 *
 *   - `restir_initial.comp` — streaming RIS over 32 candidate area-light samples drawn from
 *     the existing area-CDF NEE sampler, ranked by a luminance-of-`(f * Le * G)` target, with
 *     the shadow test folded in (`W = 0` on occlusion).
 *   - `restir_temporal.comp` — reprojects through `gbuf_prev`, validates the surface, and
 *     combines with last frame's reservoir capped at 20x the initial M.
 *   - `restir_spatial.comp` — run twice with shrinking radii (30 → 15 px, k = 5 neighbors).
 *
 * Reservoirs ping-pong between `reservoirsA` and `reservoirsB` across bindings 18/19/20. The
 * final reservoir per pixel feeds `shade_lambertian.comp`'s primary direct-lighting estimator;
 * because that estimator already accounts for visibility, the lambertian kernel sets
 * `FLAG_RESTIR_HANDLED` so `shade_emissive.comp` doesn't double-count the same path.
 */
class RestirPass : public RenderPass
{
public:
    /**
     * @brief Loads the three kernels and allocates both per-pixel reservoir buffers.
     * @param width  Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     */
    RestirPass(int width, int height);

    void uploadUniforms(const Scene&, const Camera&) override;

    /**
     * @brief Hot-reloads the three kernels, zeroing both reservoir buffers on success.
     *
     * The zeroing matters: a recompiled shader may interpret reservoir fields differently, so
     * stale history must not survive the reload.
     *
     * @return true if any of the three kernels was rebuilt.
     */
    bool reloadIfChanged(const RenderContext&) override;

    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "ReSTIR"; }

private:
    /// Zeroes both reservoir SSBOs. M = 0 is the temporal-skip signal, so a zeroed buffer
    /// correctly makes the next frame behave as if it had no history.
    void clearReservoirBuffers();

    int width;
    int height;
    int numPixels;
    int numWorkGroupsX;
    int numWorkGroupsY;

    ComputeShader initial;
    ComputeShader temporal;
    ComputeShader spatial;

    /// Two reservoir buffers ping-ponged each frame: one bound at binding 18 ("current",
    /// written this frame), the other at binding 19 ("prev", read-only this frame). Roles
    /// swap at the top of each execute(); the spatial passes additionally repurpose the prev
    /// buffer as scratch through binding 20.
    Buffer reservoirsA;
    Buffer reservoirsB;
    bool   useAAsCurrent = true;
};
