#pragma once

/**
 * @file path_tracer_pass.h
 * @brief Wavefront path tracer: per-material shade kernels driven by GPU work queues.
 */

#include <cstdint>
#include <glm/glm.hpp>

#include "gpu/buffer.h"
#include "gpu/compute_shader.h"
#include "gpu/queue.h"
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

/**
 * @brief Companion buffer for NEE plumbing (see `common/path_state.glsl`).
 *
 * Kept separate from PathState so kernels that don't touch NEE avoid the VRAM cost of
 * streaming these fields.
 */
struct alignas(16) ShadowState
{
    glm::vec3 nee_dir;  ///< Direction from the shading point toward the sampled light.
    float     nee_dist; ///< Distance to the light sample; the shadow ray's t-max.
    glm::vec3 nee_le;   ///< Radiance to add if the shadow ray is unoccluded.
    float     _pad;
};
static_assert(sizeof(ShadowState) == 32, "ShadowState size must match std430 layout");

/**
 * @brief Wavefront path tracer driven by per-material work queues.
 *
 * One PathState SSBO plus per-bounce queues turn a divergent megakernel into a sequence of
 * coherent dispatches:
 *
 *     generate          (8x8 over image)             → hit_X queues
 *     shadeLambertian   (linear over hit_lambertian) → rayQueue + shadowQueue
 *     shadeMetal                                     → rayQueue
 *     shadeDielectric                                → rayQueue
 *     shadeEmissive                                  → terminal (accumulates Le)
 *     traceShadow       (linear over shadow_queue)   → states[].radiance
 *     trace             (linear over ray_queue)      → hit_X queues  (refill)
 *     resolve           (8x8 over image)             → accum_image + normals_image
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
     * @brief Loads all nine kernels and allocates the path-state buffers and queues.
     *
     * Queues are sized for the worst case of one entry per pixel.
     *
     * @param width  Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     */
    PathTracerPass(int width, int height);

    void        uploadUniforms(const Scene&, const Camera&) override;
    bool        reloadIfChanged(const RenderContext&) override;
    void        resize(int width, int height) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "PathTracer"; }

private:
    int width;
    int height;
    int numPixels;
    int numWorkGroupsX_8x8;
    int numWorkGroupsY_8x8;
    /// Worst-case group count for the linear (local size 64) shade and trace kernels. The
    /// actual per-bounce counts come from the queues via indirect dispatch.
    int numWorkGroups_64;

    ComputeShader generate;
    ComputeShader trace;
    ComputeShader shadeLambertian;
    ComputeShader shadeMetal;
    ComputeShader shadeDielectric;
    ComputeShader shadeEmissive;
    ComputeShader traceShadow;
    ComputeShader resolve;
    /// Converts the queue counters into dispatch-indirect argument triples, so the host never
    /// has to read a counter back and stall.
    ComputeShader prepareIndirect;

    Buffer pathStateSSBO;
    Buffer shadowStateSSBO;
    /// Doubles as an SSBO (written by prepareIndirect) and as GL_DISPATCH_INDIRECT_BUFFER
    /// (read by glDispatchComputeIndirect). Holds 6 uvec3 dispatch args, indexed by SLOT_*.
    Buffer dispatchArgsSSBO;

    QueueCounters queueCounters;
    Queue         rayQueue;
    Queue         hitLambertian;
    Queue         hitMetal;
    Queue         hitDielectric;
    Queue         hitEmissive;
    Queue         shadowQueue;
};
