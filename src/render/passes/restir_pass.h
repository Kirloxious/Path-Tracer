#pragma once

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

#include "gpu/buffer.h"
#include "gpu/compute_shader.h"
#include "render/render_pass.h"

/// Mirrors `Reservoir` in restir_common.glsl (std430). Never uploaded; the static_assert pins the layout.
struct alignas(16) ReservoirData
{
    uint32_t  light_tri_idx;
    float     M;
    glm::vec2 bary;
    float     w_sum;
    float     W; ///< 0 means occluded or invalid.
    float     target_pdf;
    float     _pad;
};
static_assert(sizeof(ReservoirData) == 32, "Reservoir size must match std430 layout");
static_assert(offsetof(ReservoirData, M) == 4);
static_assert(offsetof(ReservoirData, bary) == 8);
static_assert(offsetof(ReservoirData, w_sum) == 16);
static_assert(offsetof(ReservoirData, W) == 20);
static_assert(offsetof(ReservoirData, target_pdf) == 24);

/// Mirrors `RestirSurface` in restir_surface.glsl: the surface the reservoir describes, which past a
/// mirror is the diffuse surface restir_initial walked to, not the G-buffer hit.
struct alignas(16) RestirSurfaceData
{
    glm::vec3 position;
    uint32_t  valid; ///< 0 when the pixel has no diffuse resampling vertex.
    glm::vec3 normal;
    uint32_t  matid;
    glm::vec3 view_dir;
    uint32_t  offset_n; ///< Octahedral-packed (snorm 2x16).
};
static_assert(sizeof(RestirSurfaceData) == 48, "RestirSurface size must match std430 layout");
static_assert(offsetof(RestirSurfaceData, valid) == 12);
static_assert(offsetof(RestirSurfaceData, normal) == 16);
static_assert(offsetof(RestirSurfaceData, matid) == 28);
static_assert(offsetof(RestirSurfaceData, view_dir) == 32);
static_assert(offsetof(RestirSurfaceData, offset_n) == 44);

class RestirPass : public RenderPass
{
public:
    RestirPass(int width, int height);

    void onSceneLoaded(const Scene&) override;

    /// Zeroes history on success: a recompiled shader may interpret reservoir fields differently.
    bool reloadIfChanged() override;

    void             resize(int width, int height) override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "ReSTIR"; }

private:
    void allocate(int width, int height);

    /// M = 0 is the temporal-skip signal and valid = 0 means "no resampling surface".
    void clearHistory();

    ComputeShader initial;
    ComputeShader temporal;
    ComputeShader spatial;

    /// Roles swap each frame; the spatial passes also use the prev buffer as scratch (binding 20).
    Buffer reservoirsA;
    Buffer reservoirsB;

    /// Must rotate in lockstep with the reservoirs, or a reservoir is re-weighted against a
    /// different frame's geometry.
    Buffer surfacesA;
    Buffer surfacesB;

    bool useAAsCurrent = true;
};
