#pragma once

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

#include "primitive.h"

struct OBJMesh
{
    std::vector<Vertex>     vertices;
    std::vector<glm::uvec3> indices; // one entry per triangle, each indexing into vertices
    uint32_t                material_index = 0;

    [[nodiscard]] bool empty() const { return indices.empty(); }
};

OBJMesh loadOBJ(const std::filesystem::path& path, uint32_t material_index, float scale = 1.0f, glm::vec3 offset = glm::vec3(0.0f),
                float rotateY = 0.0f);
