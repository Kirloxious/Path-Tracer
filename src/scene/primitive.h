#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

struct alignas(16) Vertex
{
    glm::vec3 position = glm::vec3(0.0f);
    float     _pad0 = 0.0f;
    glm::vec3 normal = glm::vec3(0.0f);
    // Per-vertex material id. Safe under topological-dedup-only because every triangle
    // sharing a vertex (within a sphere tessellation, a tri-quad, or one OBJ load) shares
    // a single material. Lets the raster fragment shader read matid as a flat varying
    // instead of an SSBO fetch per pixel.
    uint32_t material_index = 0;

    Vertex() = default;
    Vertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index = 0) : position(position), normal(normal), material_index(material_index) {}
};

static_assert(sizeof(Vertex) == 32, "Vertex must be 32 bytes for std430");

struct alignas(16) Triangle
{
    glm::uvec3 indices = glm::uvec3(0); // into World::vertices
    uint32_t   material_index = 0;
    glm::vec3  e1 = glm::vec3(0.0f); // v1 - v0, baked at construction
    float      area = 0.0f;          // 0.5 * |e1 x e2|, used by NEE
    glm::vec3  e2 = glm::vec3(0.0f); // v2 - v0
    // For emissive triangles: cumulative area within their light group, normalized so
    // the last triangle of each group reads exactly 1.0. Drives area-weighted NEE
    // sampling so a tessellated emissive sphere behaves like one uniform area light.
    // Zero for non-emissive triangles.
    float cdf_in_group = 0.0f;

    Triangle() = default;
};

static_assert(sizeof(Triangle) == 48, "Triangle must be 48 bytes for std430");

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

struct alignas(16) Quad
{
    glm::vec3 corner_point;
    glm::vec3 u;
    glm::vec3 v;
    uint32_t  material_index;

    Quad() = default;
    Quad(glm::vec3 corner_point, glm::vec3 u, glm::vec3 v, uint32_t material_index)
        : corner_point(corner_point), u(u), v(v), material_index(material_index) {}
};
