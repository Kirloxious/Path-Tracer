#pragma once

/**
 * @file primitive.h
 * @brief GPU-layout-compatible vertex and triangle types shared by the CPU and the shaders.
 */

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

/**
 * @brief One indexed vertex, laid out to match the std430 `Vertex` in `scene_buffers.glsl`.
 *
 * Exactly 32 bytes, `alignas(16)` — the static_assert below guards the layout contract.
 */
struct alignas(16) Vertex
{
    glm::vec3 position = glm::vec3(0.0f);
    float     _pad0 = 0.0f;
    glm::vec3 normal = glm::vec3(0.0f);
    /// Per-vertex material id. Safe under topological-dedup-only because every triangle
    /// sharing a vertex (within a sphere tessellation, a tri-quad, or one OBJ load) shares
    /// a single material. Lets the raster fragment shader read matid as a flat varying
    /// instead of an SSBO fetch per pixel.
    uint32_t material_index = 0;

    Vertex() = default;

    /**
     * @brief Constructs a vertex.
     * @param position       World-space position.
     * @param normal         Surface normal; expected normalized.
     * @param material_index Index into World::materials. Must agree with every triangle that
     *                       shares this vertex — see the field's note.
     */
    Vertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index = 0) : position(position), normal(normal), material_index(material_index) {}
};

static_assert(sizeof(Vertex) == 32, "Vertex must be 32 bytes for std430");

/**
 * @brief One triangle with precomputed edges and NEE sampling data.
 *
 * Exactly 48 bytes, `alignas(16)`. Vertex positions are not stored — the GPU fetches them
 * through `indices` — but the edges and area are baked at construction so ray-triangle
 * intersection and light sampling need no extra reads.
 *
 * Build these with makeTriangle() rather than filling the fields by hand.
 */
struct alignas(16) Triangle
{
    glm::uvec3 indices = glm::uvec3(0); ///< Indices into World::vertices.
    uint32_t   material_index = 0;
    glm::vec3  e1 = glm::vec3(0.0f); ///< v1 - v0, baked at construction.
    float      area = 0.0f;          ///< 0.5 * |e1 x e2|, used by NEE.
    glm::vec3  e2 = glm::vec3(0.0f); ///< v2 - v0.
    /// For emissive triangles: packed alias-table entry for area-weighted sampling within
    /// their light group, so a tessellated emissive sphere behaves like one uniform area
    /// light. Zero for non-emissive triangles; filled in by World::buildLightGroups().
    ///
    ///   bits 31..16  acceptance probability, as a unorm16
    ///   bits 15..0   alias target, as an offset from LightGroup::begin
    ///
    /// This replaced a cumulative-area CDF that NEE binary-searched. That search was a
    /// chain of dependent scattered loads — log2(count) of them per candidate, each
    /// pulling a whole 48-byte Triangle to read one float — and restir_initial draws 32
    /// candidates per pixel. An alias table samples the same distribution in O(1) from a
    /// single load. It reuses the CDF's slot so Triangle stays 48 bytes and needs no new
    /// SSBO binding; shade_lambertian is already at 15 of NVIDIA's 16.
    ///
    /// The 16-bit alias offset caps a light group at 65535 triangles;
    /// World::buildLightGroups() splits longer runs so that stays true.
    uint32_t alias_packed = 0u;

    Triangle() = default;
};

static_assert(sizeof(Triangle) == 48, "Triangle must be 48 bytes for std430");

/**
 * @brief Builds a Triangle, baking its edge vectors and area.
 *
 * Winding matters: the NEE light-pdf code and ReSTIR's target pdf both gate on
 * `dot(cross(e1, e2), light_dir) < 0` to detect receiver-facing light samples, so emissive
 * geometry must be wound counter-clockwise relative to its outward normal.
 *
 * @param vertices       Vertex pool the indices refer into; the three entries must exist.
 * @param i0             First vertex index.
 * @param i1             Second vertex index.
 * @param i2             Third vertex index.
 * @param material_index Index into World::materials, matching all three vertices'.
 * @return The constructed triangle. `alias_packed` is left at 0 for World::buildLightGroups().
 */
inline Triangle makeTriangle(const std::vector<Vertex>& vertices, uint32_t i0, uint32_t i1, uint32_t i2, uint32_t material_index) {
    Triangle t;
    t.indices = glm::uvec3(i0, i1, i2);
    t.material_index = material_index;
    const glm::vec3& p0 = vertices[i0].position;
    const glm::vec3& p1 = vertices[i1].position;
    const glm::vec3& p2 = vertices[i2].position;
    t.e1 = p1 - p0;
    t.e2 = p2 - p0;
    t.area = 0.5f * glm::length(glm::cross(t.e1, t.e2));
    return t;
}
