#pragma once

/**
 * @file obj_loader.h
 * @brief Wavefront OBJ import on top of tinyobjloader.
 */

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/mesh.h"

/**
 * @brief Loads a `.obj` file as a Mesh, applying a rotate → scale → translate transform.
 *
 * Vertices are deduplicated by (position index, normal index), so a shared corner is stored
 * once. Per-face OBJ materials are ignored — the material comes from World::addMesh() or the
 * Object placing the asset. When the OBJ carries no `vn` lines, smooth per-vertex normals are
 * synthesized by averaging adjacent face normals.
 *
 * Feed addMeshAsset() a load with the default scale/offset/rotation, or the transform is baked
 * into the asset and every placement inherits it.
 *
 * @param path    Path to the OBJ file, relative to the working directory.
 * @param scale   Uniform scale applied after rotation.
 * @param offset  Translation applied last.
 * @param rotateY Rotation about the Y axis in radians, applied to positions and normals
 *                before scaling.
 * @return The mesh, named after the file stem, or why it could not be loaded — a missing
 *         file, a parse error, or a file with no triangles.
 */
std::expected<Mesh, std::string> loadOBJ(const std::filesystem::path& path, float scale = 1.0f, glm::vec3 offset = glm::vec3(0.0f),
                                         float rotateY = 0.0f);
