#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/primitive.h"

/// Carries no material or placement; World::create() bakes the placing Object's in, overwriting
/// `material_index`, which is what lets one asset be placed under two materials.
struct Mesh
{
    std::string             name;
    std::vector<Vertex>     vertices; ///< `material_index` unused.
    std::vector<glm::uvec3> indices;

    [[nodiscard]] bool empty() const { return indices.empty(); }

    [[nodiscard]] std::size_t triangleCount() const { return indices.size(); }
};

/// Unit radius so one asset serves every sphere of that density. Wound CCW about the outward normal,
/// which the light pdf and ReSTIR target pdf rely on.
[[nodiscard]] Mesh makeUnitSphereMesh(int latSegs = 8, int lonSegs = 16);

/// Wound CCW around `cross(u, v)`, so swapping @p u and @p v flips the facing.
[[nodiscard]] Mesh makeQuadMesh(glm::vec3 u, glm::vec3 v, std::string name = "Quad");

/// Wind CCW about the intended outward normal.
[[nodiscard]] Mesh makeTriangleMesh(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, std::string name = "Triangle");
