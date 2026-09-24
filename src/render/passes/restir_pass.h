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
 * @brief CPU-side mirror of `RestirSurface` in `shader/common/restir_surface.glsl`.
 *
 * The surface a pixel's reservoir describes. For a diffuse primary that is the G-buffer hit;
 * past a mirror it is the reflected diffuse surface `restir_initial.comp` walked to, which
 * the G-buffer knows nothing about. Never uploaded from the CPU — the static_assert exists to
 * pin the layout.
 */
struct alignas(16) RestirSurfaceData
{
    glm::vec3 position; ///< World-space resampling vertex.
    uint32_t  valid;    ///< 0 when this pixel has no diffuse resampling vertex.
    glm::vec3 normal;   ///< Shading normal there.
    uint32_t  matid;    ///< Material at the resampling vertex, for reuse validation.
    glm::vec3 view_dir; ///< Unit vector toward the viewer, for the BRDF.
    uint32_t  offset_n; ///< Octahedral-packed ray-origin offset normal (snorm 2x16).
};
static_assert(sizeof(RestirSurfaceData) == 48, "RestirSurface size must match std430 layout");

/**
 * @brief ReSTIR DI at primary surfaces, producing one reservoir per pixel.
 *
 * Three back-to-back compute stages:
 *
 *   - `restir_initial.comp` — streaming RIS over 32 candidate area-light samples drawn from
 *     the light-group alias tables, ranked by a luminance-of-`(f * Le * G)` target, with the
 *     shadow test folded in (`W = 0` on occlusion). Past a mirror it walks the specular chain
 *     to the first diffuse surface and records that as the pixel's resampling surface.
 *   - `restir_temporal.comp` — reprojects and validates against last frame's resampling
 *     surfaces, then combines with last frame's reservoir capped at 20x the initial M.
 *   - `restir_spatial.comp` — run twice with shrinking radii (30 → 15 px, k = 5 neighbors).
 *
 * Reservoirs ping-pong between `reservoirsA` and `reservoirsB` across bindings 18/19/20, and
 * the resampling surfaces between `surfacesA`/`surfacesB` at 24/25. The final reservoir per
 * pixel is `shade_opaque.comp`'s direct-lighting estimator at that surface, which sets
 * `FLAG_RESTIR_HANDLED` so `shade_emissive.comp` doesn't count the same light twice.
 */
class RestirPass : public RenderPass
{
public:
    /**
     * @brief Loads the three kernels and allocates the reservoir and surface buffers.
     * @param width  Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     */
    RestirPass(int width, int height);

    /// A new scene invalidates every reservoir.
    void onSceneLoaded(const Scene&) override;

    /**
     * @brief Hot-reloads the three kernels, zeroing the history buffers on success.
     *
     * The zeroing matters: a recompiled shader may interpret reservoir fields differently, so
     * stale history must not survive the reload.
     *
     * @return true if any of the three kernels was rebuilt.
     */
    bool reloadIfChanged() override;

    void             resize(int width, int height) override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "ReSTIR"; }

private:
    /// Allocates and zeroes all four per-pixel buffers.
    void allocate(int width, int height);

    /// Zeroes the reservoir and surface buffers. M = 0 is the temporal-skip signal and
    /// valid = 0 means "no resampling surface", so the next frame sees no history.
    void clearHistory();

    ComputeShader initial;
    ComputeShader temporal;
    ComputeShader spatial;

    /// Two reservoir buffers ping-ponged each frame: one bound at binding 18 ("current",
    /// written this frame), the other at binding 19 ("prev", read-only this frame). Roles
    /// swap at the end of each execute(); the spatial passes additionally repurpose the prev
    /// buffer as scratch through binding 20.
    Buffer reservoirsA;
    Buffer reservoirsB;

    /// Resampling surfaces, ping-ponged alongside the reservoirs: binding 24 is this
    /// frame's (written by `restir_initial`, read by the spatial passes), binding 25 is
    /// last frame's, which `restir_temporal` validates reuse against. They must rotate in
    /// lockstep with the reservoirs — a reservoir paired with the wrong frame's surface
    /// would be re-weighted against geometry it never saw.
    Buffer surfacesA;
    Buffer surfacesB;

    bool useAAsCurrent = true;
};
