#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "gpu/buffer.h"
#include "core/shader_shared.h"
#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/// Mirrors `common/path_state.glsl` (std430): every `vec3 + scalar` pair is one 16-byte slot.
struct alignas(16) PathState
{
    glm::vec3 throughput;
    uint32_t  flags;
    glm::vec3 radiance;
    uint32_t  rng_state; ///< Per-pixel seed; must not vary per frame.
    glm::vec3 ray_origin;
    float     pdf_bsdf; ///< For MIS.
    glm::vec3 ray_dir;
    uint32_t  bounce;
    glm::vec3 hit_point;
    uint32_t  hit_matid;
    glm::vec3 hit_normal; ///< Face-corrected shading normal.
    uint32_t  hit_triangle_idx;
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

/// Mirrors `common/shadow_state.glsl`. Separate from PathState so kernels without NEE don't stream it.
struct alignas(16) ShadowState
{
    glm::vec3 nee_dir;
    float     nee_dist;
    glm::vec3 nee_le;
    float     nee_valid;
    glm::vec3 env_dir;
    float     env_valid;
    glm::vec3 env_le;
    uint32_t  nee_tri; ///< Exempt from its own shadow test.
};
static_assert(sizeof(ShadowState) == 64, "ShadowState size must match std430 layout");
static_assert(offsetof(ShadowState, nee_dist) == 12);
static_assert(offsetof(ShadowState, nee_le) == 16);
static_assert(offsetof(ShadowState, nee_valid) == 28);
static_assert(offsetof(ShadowState, env_dir) == 32);
static_assert(offsetof(ShadowState, env_valid) == 44);
static_assert(offsetof(ShadowState, env_le) == 48);
static_assert(offsetof(ShadowState, nee_tri) == 60);

class PathTracerPass : public RenderPass
{
public:
    /// Queues are sized for the worst case of one entry per pixel.
    PathTracerPass(int width, int height);

    bool             reloadIfChanged() override;
    void             resize(int width, int height) override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "PathTracer"; }

private:
    void allocate(int width, int height);

    ComputeShader generate;
    ComputeShader trace;
    ComputeShader shadeOpaque;
    ComputeShader shadeTransmissive;
    ComputeShader shadeEmissive;
    ComputeShader traceShadow;
    ComputeShader resolve;
    /// Lets the host dispatch from queue counters without reading them back.
    ComputeShader prepareIndirect;

    Buffer pathStateSSBO;
    Buffer shadowStateSSBO;
    /// Written as an SSBO, read as GL_DISPATCH_INDIRECT_BUFFER; one uvec4 per queue, indexed by Q_*.
    Buffer dispatchArgsSSBO;
    /// One SSBO for all counters; see queue.glsl for why.
    Buffer                         queueCounters;
    std::array<Buffer, NUM_QUEUES> queueIndices;
};
