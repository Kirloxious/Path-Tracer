#pragma once

/**
 * @file obj_loader.h
 * @brief Wavefront OBJ import on top of tinyobjloader.
 */

#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/primitive.h"

/**
 * @brief An indexed triangle mesh produced by loadOBJ(), ready for World::addMesh().
 *
 * Vertices are deduplicated by (position index, normal index) so a shared corner is stored
 * once — which is what lets the raster pass use a flat per-vertex material id.
 */
struct OBJMesh
{
    /// Source file stem, e.g. "suzanne" for `assets/suzanne.obj`. Becomes Mesh::name.
    std::string             name;
    std::vector<Vertex>     vertices;
    std::vector<glm::uvec3> indices; ///< One entry per triangle, each indexing into `vertices`.
    uint32_t                material_index = 0;

    /// @return true when the mesh has no triangles — the signal that loading failed or the
    ///         file was empty. Callers should skip World::addMesh() in that case.
    [[nodiscard]] bool empty() const { return indices.empty(); }
};

/**
 * @brief Loads a `.obj` file, applying a rotate → scale → translate transform to every vertex.
 *
 * Missing files, parse errors and empty results are reported through Log:: and returned as an
 * empty OBJMesh — nothing throws. When the OBJ carries no `vn` lines, smooth per-vertex normals
 * are synthesized by averaging adjacent face normals.
 *
 * @param path           Path to the OBJ file, relative to the working directory.
 * @param material_index Index into World::materials applied to every vertex and triangle;
 *                       per-face OBJ materials are ignored.
 * @param scale          Uniform scale applied after rotation. Non-positive values are
 *                       warned about and produce degenerate geometry.
 * @param offset         World-space translation applied last.
 * @param rotateY        Rotation about the Y axis in radians, applied to positions and normals
 *                       before scaling.
 * @return The loaded mesh, or an empty OBJMesh on failure (check OBJMesh::empty()).
 */
OBJMesh loadOBJ(const std::filesystem::path& path, uint32_t material_index, float scale = 1.0f, glm::vec3 offset = glm::vec3(0.0f),
                float rotateY = 0.0f);
