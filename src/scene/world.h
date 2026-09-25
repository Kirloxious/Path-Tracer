#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "scene/bvh.h"
#include "scene/material.h"
#include "scene/mesh.h"

inline constexpr uint32_t NO_MESH = 0xFFFFFFFFu;

inline constexpr uint32_t NO_OBJECT = 0xFFFFFFFFu;

/// CPU-only authoring unit: create() bakes each into world-space arrays. Objects sharing a `meshId`
/// still get their own baked vertices.
struct Object
{
    std::string name;
    uint32_t    meshId = NO_MESH; ///< NO_MESH for baked immediate-mode geometry.
    glm::mat4   transform{1.0f};
    uint32_t    material_index = 0;
    /// Use `triangleObjectId` to find which triangles; sortEmissiveFirst() permutes them.
    uint32_t triangleCount = 0;
};

class World
{
public:
    /// Mirrors GLSL `LightGroup`. Groups are selected by power, not uniformly, so every caller multiplies
    /// by `selectPdf` rather than dividing by the group count.
    struct alignas(16) LightGroup
    {
        int32_t  begin = 0;
        int32_t  count = 0;
        float    totalArea = 0.0f;
        uint32_t aliasPacked = 0;  ///< Alias entry over groups: bits 31..16 unorm16 accept, 15..0 alias target.
        float    selectPdf = 0.0f; ///< Power / total power.
        float    _pad0 = 0.0f;
        float    _pad1 = 0.0f;
        float    _pad2 = 0.0f;
    };

    std::vector<Vertex>     vertices;
    std::vector<Triangle>   triangles;
    std::vector<Material>   materials;
    std::vector<LightGroup> lightGroups;
    BVH                     bvh;

    std::vector<Mesh> meshes;
    /// Includes one auto-registered entry per immediate-mode builder call, so every triangle has an owner.
    std::vector<Object> objects;
    /// Parallel to `triangles`; permuted in lockstep by sortEmissiveFirst().
    std::vector<uint32_t> triangleObjectId;
    /// -1 when the scene has no emitters.
    int emissiveLastIndex = -1;

    uint32_t addMaterial(Material mat);

    /// @p material_index must match every triangle sharing this vertex (read as a flat varying).
    uint32_t addVertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index);

    uint32_t addMeshAsset(Mesh mesh);

    /// Deferred until create(). A negative-determinant transform has its winding flipped on
    /// instantiation to keep emitters CCW about their outward normal.
    uint32_t addObject(std::string name, uint32_t meshId, const glm::mat4& transform, uint32_t material_index);

    uint32_t addObject(std::string name, uint32_t meshId, const glm::mat4& transform, Material mat);

    /// Wound CCW about the outward normal. Every triangle lands in the BVH, so small spheres should
    /// pass a lower density.
    void addSphere(glm::vec3 center, float radius, uint32_t material_index, int latSegs = 8, int lonSegs = 16);

    void addSphere(glm::vec3 center, float radius, Material mat, int latSegs = 8, int lonSegs = 16);

    /// Wind CCW about the intended outward normal.
    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index);

    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Material mat);

    /// Wound CCW around `cross(u, v)`: swapping @p u and @p v flips the facing, which matters for emitters.
    void addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, uint32_t material_index);

    void addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, Material mat);

    void addMesh(const Mesh& mesh, uint32_t material_index);

    /// Owns the build order because the emissive sort must follow instantiation. Afterwards every
    /// `add*` builder throws. Throws std::runtime_error on invalid geometry, std::logic_error if called twice.
    void create();

    [[nodiscard]] bool isCreated() const { return created; }

private:
    void instantiateObjects();

    /// NEE assumes a contiguous emissive prefix.
    void sortEmissiveFirst();

    [[nodiscard]] std::expected<void, std::string> validate() const;

    void requireEditable(std::string_view builder) const;

    /// Every immediate-mode builder ends with one, so no consumer needs an unowned-geometry case.
    uint32_t recordImmediateObject(std::string name, std::size_t firstTriangle);

    bool created = false;

    /// Coalesces consecutive emissive triangles sharing a material into one light, so two distinct lights
    /// with the same appearance must use separate materials.
    void buildLightGroups();

    void buildLightGroupSelection();
};

static_assert(sizeof(World::LightGroup) == 32, "LightGroup must match std430 layout");
static_assert(offsetof(World::LightGroup, totalArea) == 8);
static_assert(offsetof(World::LightGroup, aliasPacked) == 12);
static_assert(offsetof(World::LightGroup, selectPdf) == 16);
