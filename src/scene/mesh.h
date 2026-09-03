#pragma once

/**
 * @file mesh.h
 * @brief Reusable object-space geometry assets and the built-in shape builders.
 */

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/obj_loader.h"
#include "scene/primitive.h"

/**
 * @brief An indexed triangle mesh in object space, stored once and instantiated many times.
 *
 * Carries no material and no placement — both live on the Object that references it, and
 * World::create() bakes them in. `Vertex::material_index` is ignored here; instantiation
 * overwrites it, which is what lets one asset be placed under two materials.
 */
struct Mesh
{
    std::string             name;     ///< Display name, shown in the GUI object list.
    std::vector<Vertex>     vertices; ///< Object-space vertices; `material_index` unused.
    std::vector<glm::uvec3> indices;  ///< One entry per triangle, each indexing into `vertices`.

    /// @return true when the mesh has no triangles — instantiation skips it.
    [[nodiscard]] bool empty() const { return indices.empty(); }

    /// @return Triangle count contributed by each instance.
    [[nodiscard]] std::size_t triangleCount() const { return indices.size(); }
};

/**
 * @brief Tessellates a unit-radius UV sphere centred on the origin.
 *
 * Unit-sized so every sphere at a given density can share one asset and take its size from
 * the Object's scale. Wound CCW relative to the outward normal, which the light-pdf and
 * ReSTIR target-pdf code relies on.
 *
 * @param latSegs Latitude segments; clamped to a minimum of 2.
 * @param lonSegs Longitude segments; clamped to a minimum of 3.
 * @return The tessellated sphere, named after its density.
 */
[[nodiscard]] Mesh makeUnitSphereMesh(int latSegs = 8, int lonSegs = 16);

/**
 * @brief Builds a parallelogram as two triangles, origin corner at the origin.
 *
 * Both triangles are wound CCW around `cross(u, v)`, so swapping @p u and @p v flips which
 * way the quad faces.
 *
 * @param u    First edge vector from the origin corner.
 * @param v    Second edge vector from the origin corner.
 * @param name Display name for the asset.
 * @return The two-triangle quad.
 */
[[nodiscard]] Mesh makeQuadMesh(glm::vec3 u, glm::vec3 v, std::string name = "Quad");

/**
 * @brief Builds a single triangle with a flat geometric normal.
 * @param v0   First corner.
 * @param v1   Second corner.
 * @param v2   Third corner. Wind CCW about the intended outward normal.
 * @param name Display name for the asset.
 * @return The one-triangle mesh.
 */
[[nodiscard]] Mesh makeTriangleMesh(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, std::string name = "Triangle");

/**
 * @brief Adapts a loaded OBJ into a Mesh asset, dropping its material.
 * @param mesh Mesh from loadOBJ(). An empty mesh produces an empty asset.
 * @return The converted asset, named after OBJMesh::name.
 */
[[nodiscard]] Mesh makeMeshFromOBJ(const OBJMesh& mesh);
