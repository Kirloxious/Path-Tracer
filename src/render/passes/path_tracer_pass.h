#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "gpu/buffer.h"
#include "gpu/compute_shader.h"
#include "gpu/queue.h"
#include "render/render_pass.h"

// CPU-visible mirror of common/path_state.glsl. Layout must match the std430
// declaration exactly: every `vec3 + scalar` pair is one 16-byte slot.
struct alignas(16) PathState
{
    glm::vec3 throughput;
    uint32_t  flags;
    glm::vec3 radiance;
    uint32_t  rng_state;
    glm::vec3 ray_origin;
    float     pdf_bsdf;
    glm::vec3 ray_dir;
    uint32_t  bounce;
    glm::vec3 hit_point;
    uint32_t  hit_matid;
    glm::vec3 hit_normal;
    uint32_t  hit_triangle_idx;
};
static_assert(sizeof(PathState) == 96, "PathState size must match std430 layout");

// Companion buffer for NEE plumbing (see common/path_state.glsl). Kept separate
// from PathState so kernels that don't touch NEE avoid the VRAM cost.
struct alignas(16) ShadowState
{
    glm::vec3 nee_dir;
    float     nee_dist;
    glm::vec3 nee_le;
    float     _pad;
};
static_assert(sizeof(ShadowState) == 32, "ShadowState size must match std430 layout");

// Wavefront path tracer. One PathState SSBO plus per-bounce queues drive a
// sequence of coherent compute dispatches:
//
//   generate          (8x8 over image)             → hit_X queues
//   shadeLambertian   (linear over hit_lambertian) → rayQueue + shadowQueue
//   shadeMetal                                     → rayQueue
//   shadeDielectric                                → rayQueue
//   shadeEmissive                                  → terminal (accumulates Le)
//   traceShadow       (linear over shadow_queue)   → states[].radiance
//   trace             (linear over ray_queue)      → hit_X queues  (refill)
//   resolve           (8x8 over image)             → accum_image + normals_image
//
// Sharing happens via SSBO binding numbers, not explicit data passing — every
// stage shader includes common/path_state.glsl and common/queue.glsl.
class PathTracerPass : public RenderPass
{
public:
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
    int numWorkGroups_64;

    ComputeShader generate;
    ComputeShader trace;
    ComputeShader shadeLambertian;
    ComputeShader shadeMetal;
    ComputeShader shadeDielectric;
    ComputeShader shadeEmissive;
    ComputeShader traceShadow;
    ComputeShader resolve;
    ComputeShader prepareIndirect;

    Buffer pathStateSSBO;
    Buffer shadowStateSSBO;
    // Doubles as an SSBO (written by prepareIndirect) and as GL_DISPATCH_INDIRECT_BUFFER
    // (read by glDispatchComputeIndirect). Holds 6 uvec3 dispatch args, indexed by SLOT_*.
    Buffer dispatchArgsSSBO;

    QueueCounters queueCounters;
    Queue         rayQueue;
    Queue         hitLambertian;
    Queue         hitMetal;
    Queue         hitDielectric;
    Queue         hitEmissive;
    Queue         shadowQueue;
};
