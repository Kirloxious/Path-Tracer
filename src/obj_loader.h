#pragma once

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

#include "primitive.h"

struct OBJMesh
{
    std::vector<Triangle> triangles;

    [[nodiscard]] bool empty() const { return triangles.empty(); }
};

OBJMesh loadOBJ(const std::filesystem::path& path, uint32_t material_index, float scale = 1.0f, glm::vec3 offset = glm::vec3(0.0f),
                float rotateY = 0.0f);