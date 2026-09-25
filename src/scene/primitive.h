#pragma once

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <span>

/// Mirrors std430 `Vertex` in scene_buffers.glsl.
struct alignas(16) Vertex
{
    glm::vec3 position = glm::vec3(0.0f);
    float     _pad0 = 0.0f;
    glm::vec3 normal = glm::vec3(0.0f);
    /// Safe per-vertex because every triangle sharing a vertex shares its material; lets the raster
    /// shader read matid as a flat varying instead of an SSBO fetch.
    uint32_t material_index = 0;

    Vertex() = default;

    Vertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index = 0) : position(position), normal(normal), material_index(material_index) {}
};

static_assert(sizeof(Vertex) == 32, "Vertex must be 32 bytes for std430");
static_assert(offsetof(Vertex, normal) == 16);
static_assert(offsetof(Vertex, material_index) == 28);

/// Mirrors std430 `Triangle`. Build with makeTriangle(); edges and area are baked so intersection
/// and light sampling need no extra reads.
struct alignas(16) Triangle
{
    glm::uvec3 indices = glm::uvec3(0);
    uint32_t   material_index = 0;
    glm::vec3  e1 = glm::vec3(0.0f);
    float      area = 0.0f;
    glm::vec3  e2 = glm::vec3(0.0f);
    /// Emissive only: alias entry within the light group; bits 31..16 = accept (unorm16), 15..0 = alias
    /// offset from LightGroup::begin, capping a group at 65535 triangles.
    uint32_t alias_packed = 0u;

    Triangle() = default;
};

static_assert(sizeof(Triangle) == 48, "Triangle must be 48 bytes for std430");
static_assert(offsetof(Triangle, material_index) == 12);
static_assert(offsetof(Triangle, e1) == 16);
static_assert(offsetof(Triangle, area) == 28);
static_assert(offsetof(Triangle, e2) == 32);
static_assert(offsetof(Triangle, alias_packed) == 44);

/// Emissive geometry must wind CCW about its outward normal: NEE and ReSTIR detect back-facing
/// light samples with `dot(cross(e1, e2), light_dir) < 0`.
inline Triangle makeTriangle(std::span<const Vertex> vertices, uint32_t i0, uint32_t i1, uint32_t i2, uint32_t material_index) {
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
