#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/mesh.h"

/// Rotate -> scale -> translate. OBJ materials are ignored; missing normals are synthesized by averaging
/// face normals. Feed addMeshAsset() an untransformed load, or every placement inherits the transform.
std::expected<Mesh, std::string> loadOBJ(const std::filesystem::path& path, float scale = 1.0f, glm::vec3 offset = glm::vec3(0.0f),
                                         float rotateY = 0.0f);
