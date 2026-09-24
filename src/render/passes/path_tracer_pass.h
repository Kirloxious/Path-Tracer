#pragma once

/**
 * @file path_tracer_pass.h
 * @brief Wavefront path tracer: per-material shade kernels driven by GPU work queues.
 */

#include <cstddef>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "gpu/buffer.h"
#include "core/shader_shared.h"
#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/**
 * @brief CPU-visible mirror of `common/path_state.glsl`.
 *
 * One entry per pixel, persisting across every bounce of a frame. Layout must match the std430
 * declaration exactly: every `vec3 + scalar` pair is one 16-byte slot.
 */
struct alignas(16) PathState
{
    glm::vec3 throughput;       ///< Accumulated BSDF/pdf product along the path so far.
    uint32_t  flags;            ///< FLAG_RESTIR_HANDLED / FLAG_PREV_NON_SPECULAR, see restir_common.glsl.
    glm::vec3 radiance;         ///< Radiance gathered so far; resolve.comp folds this into `accum`.
    uint32_t  rng_state;        ///< Per-path RNG state, advanced by every sampling decision.
    glm::vec3 ray_origin;       ///< Origin of the continuation ray.
    float     pdf_bsdf;         ///< Solid-angle pdf of the last BSDF sample, for MIS.
    glm::vec3 ray_dir;          ///< Direction of the continuation ray.
    uint32_t  bounce;           ///< Bounce depth, 0 at the primary hit.
    glm::vec3 hit_point;        ///< World-space position of the current hit.
    uint32_t  hit_matid;        ///< Material index at the current hit.
    glm::vec3 hit_normal;       ///< Shading normal at the current hit, face-corrected.
    uint32_t  hit_triangle_idx; ///< Triangle index of the current hit.
};
static_assert(sizeof(PathState) == 96, "PathState size must match std430 layout");
static_assert(offsetof(PathState, flags) == 12);
static_assert(offsetof(PathState, radiance) == 16);
static_assert(offsetof(PathState, rng_state) == 28);
static_assert(offsetof(PathState, ray_origin) == 32);
static_assert(offsetof(PathState, pdf_bsdf) == 44);
static_assert(offsetof(PathState, ray_dir) == 48);
static_assert(offsetof(PathState, bounce) == 60);
static_assert(offsetof(PathState, hit_point) == 64);
static_assert(offsetof(PathState, hit_matid) == 76);
static_assert(offsetof(PathState, hit_normal) == 80);
static_assert(offsetof(PathState, hit_triangle_idx) == 92);

/**
 * @brief Companion buffer for NEE plumbing (see `common/path_state.glsl`).
 *
 * Kept separate from PathState so kernels that don't touch NEE avoid the VRAM cost of
 * streaming these fields. Two independent slots — area lights and the environment sample
 * disjoint parts of the integrand, so a vertex can schedule one of each; see
 * `shader/common/shadow_state.glsl`.
 */
struct alignas(16) ShadowState
{
    glm::vec3 nee_dir;   ///< Direction from the shading point toward the sampled light.
    float     nee_dist;  ///< Distance to the light sample; the shadow ray's t-max.
    glm::vec3 nee_le;    ///< Radiance to add if the light shadow ray is unoccluded.
    float     nee_valid; ///< Non-zero when the light slot holds a scheduled sample.
    glm::vec3 env_dir;   ///< Direction toward the sampled point on the environment.
    float     env_valid; ///< Non-zero when the environment slot holds a scheduled sample.
    glm::vec3 env_le;    ///< Radiance to add if the environment shadow ray is unoccluded.
    uint32_t  nee_tri;   ///< Sampled light triangle, exempt from its own shadow test.
};
static_assert(sizeof(ShadowState) == 64, "ShadowState size must match std430 layout");
static_assert(offsetof(ShadowState, nee_dist) == 12);
static_assert(offsetof(ShadowState, nee_le) == 16);
static_assert(offsetof(ShadowState, nee_valid) == 28);
static_assert(offsetof(ShadowState, env_dir) == 32);
static_assert(offsetof(ShadowState, env_valid) == 44);
static_assert(offsetof(ShadowState, env_le) == 48);
static_assert(offsetof(ShadowState, nee_tri) == 60);

/**
 * @brief Wavefront path tracer driven by per-material work queues.
 *
 * One PathState SSBO plus per-bounce queues turn a divergent megakernel into a sequence of
 * coherent dispatches:
 *
 *     generate           (8x8 over image)                → hit_X queues
 *     shadeOpaque        (linear over hit_opaque)        → ray + shadow queues
 *     shadeTransmissive  (linear over hit_transmissive)  → ray + shadow queues
 *     shadeEmissive      (linear over hit_emissive)      → terminal (accumulates Le)
 *     traceShadow        (linear over the shadow queue)  → states[].radiance
 *     trace              (linear over the ray queue)     → hit_X queues (refill)
 *     resolve            (8x8 over image)                → accum + normals + moments
 *
 * The shade/trace block repeats `max_bounces` times before resolve. `generate.comp` seeds
 * PathState straight from the G-buffer and routes the primary hit into the right queue without
 * ever calling the BVH — there is no GPU primary ray cast.
 *
 * Sharing happens via SSBO binding numbers, not explicit data passing — every stage shader
 * includes `common/path_state.glsl` and `common/queue.glsl`.
 */
class PathTracerPass : public RenderPass
{
public:
    /**
     * @brief Loads all eight kernels and allocates the path-state buffers and queues.
     *
     * Queues are sized for the worst case of one entry per pixel.
     *
     * @param width  Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     */
    PathTracerPass(int width, int height);

    bool             reloadIfChanged() override;
    void             resize(int width, int height) override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "PathTracer"; }

private:
    /// Allocates every per-pixel buffer for a @p width x @p height image.
    void allocate(int width, int height);

    ComputeShader generate;
    ComputeShader trace;
    ComputeShader shadeOpaque;
    ComputeShader shadeTransmissive;
    ComputeShader shadeEmissive;
    ComputeShader traceShadow;
    ComputeShader resolve;
    /// Converts the queue counters into dispatch-indirect argument triples, so the host never
    /// has to read a counter back and stall.
    ComputeShader prepareIndirect;

    Buffer pathStateSSBO;
    Buffer shadowStateSSBO;
    /// Doubles as an SSBO (written by prepareIndirect) and as GL_DISPATCH_INDIRECT_BUFFER
    /// (read by glDispatchComputeIndirect). One uvec4 of dispatch args per queue, indexed by Q_*.
    Buffer dispatchArgsSSBO;
    /// One uint counter per queue in a single SSBO; see queue.glsl for why they are shared.
    Buffer queueCounters;
    /// Path indices per queue, indexed by Q_*.
    std::array<Buffer, NUM_QUEUES> queueIndices;
};
