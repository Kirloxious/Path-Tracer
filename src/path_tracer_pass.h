#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "buffer.h"
#include "compute_shader.h"
#include "queue.h"
#include "render_pass.h"

// CPU-visible mirror of common/path_state.glsl. Layout must match the std430
// declaration exactly: every `vec3 + scalar` pair is one 16-byte slot.
struct alignas(16) PathState
{
    glm::vec3  throughput;
    uint32_t   flags;
    glm::vec3  radiance;
    uint32_t   pixel_idx;
    glm::uvec4 rng_state;
    glm::vec3  ray_origin;
    float      pdf_bsdf;
    glm::vec3  ray_dir;
    uint32_t   bounce;
    glm::vec3  hit_point;
    uint32_t   hit_matid;
    glm::vec3  hit_normal;
    float      hit_t;
    glm::vec3  nee_dir;
    float      nee_dist;
    glm::vec3  nee_le;
    float      _pad;
};
static_assert(sizeof(PathState) == 144, "PathState size must match std430 layout");

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

    void uploadUniforms(const RenderContext&) override;
    bool reloadIfChanged(const RenderContext&) override;
    void execute(const RenderContext&, RenderTargets&) override;

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

    Buffer pathStateSSBO;

    QueueCounters queueCounters;
    Queue         rayQueue;
    Queue         hitLambertian;
    Queue         hitMetal;
    Queue         hitDielectric;
    Queue         hitEmissive;
    Queue         shadowQueue;
};
