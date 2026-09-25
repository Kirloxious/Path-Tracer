#include "scene/obj_loader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <unordered_map>

#include "core/log.h"

namespace {
struct VertexKey
{
    int vIdx;
    int nIdx;

    bool operator==(const VertexKey& other) const { return vIdx == other.vIdx && nIdx == other.nIdx; }
};

struct VertexKeyHash
{
    std::size_t operator()(const VertexKey& k) const noexcept {
        std::uint64_t a = static_cast<std::uint32_t>(k.vIdx);
        std::uint64_t b = static_cast<std::uint32_t>(k.nIdx);
        std::uint64_t h = (a << 32) ^ b;
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        return static_cast<std::size_t>(h);
    }
};
} // namespace

std::expected<Mesh, std::string> loadOBJ(const std::filesystem::path& path, float scale, glm::vec3 offset, float rotateY) {
    Mesh mesh;
    mesh.name = path.stem().string();

    if (!std::filesystem::exists(path)) {
        return std::unexpected(std::format("OBJ file does not exist: {}", path.string()));
    }
    if (scale <= 0.0f) {
        return std::unexpected(std::format("OBJ '{}' requested with non-positive scale {}", path.string(), scale));
    }

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.string().c_str())) {
        return std::unexpected(std::format("Failed to load OBJ file: {} — {}", path.string(), err));
    }
    if (!err.empty()) {
        Log::warn("OBJ: {}", err);
    }

    const bool  hasNormals = !attrib.normals.empty();
    const float cosY = std::cos(rotateY), sinY = std::sin(rotateY);
    auto        rotY = [&](glm::vec3 v) -> glm::vec3 {
        return glm::vec3(cosY * v.x + sinY * v.z, v.y, -sinY * v.x + cosY * v.z);
    };

    // Some faces may carry `vn` and others not, so the fallback applies if *any* face vertex lacks one.
    const bool needsSmoothNormals = !hasNormals || std::any_of(shapes.begin(), shapes.end(), [](const tinyobj::shape_t& s) {
        return std::any_of(s.mesh.indices.begin(), s.mesh.indices.end(), [](const tinyobj::index_t& i) { return i.normal_index < 0; });
    });

    std::vector<glm::vec3> smoothNormals;
    if (needsSmoothNormals) {
        smoothNormals.resize(attrib.vertices.size() / 3, glm::vec3(0.0f));

        for (const auto& shape : shapes) {
            size_t indexOffset = 0;
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
                int fv = shape.mesh.num_face_vertices[f];
                if (fv < 3) {
                    indexOffset += fv;
                    continue;
                }

                auto      idx0 = shape.mesh.indices[indexOffset];
                auto      idx1 = shape.mesh.indices[indexOffset + 1];
                auto      idx2 = shape.mesh.indices[indexOffset + 2];
                glm::vec3 v0(
                    attrib.vertices[3 * idx0.vertex_index], attrib.vertices[3 * idx0.vertex_index + 1], attrib.vertices[3 * idx0.vertex_index + 2]);
                glm::vec3 v1(
                    attrib.vertices[3 * idx1.vertex_index], attrib.vertices[3 * idx1.vertex_index + 1], attrib.vertices[3 * idx1.vertex_index + 2]);
                glm::vec3 v2(
                    attrib.vertices[3 * idx2.vertex_index], attrib.vertices[3 * idx2.vertex_index + 1], attrib.vertices[3 * idx2.vertex_index + 2]);

                // Unnormalized cross product: weights by face area.
                glm::vec3 faceNormal = glm::cross(v1 - v0, v2 - v0);

                for (int i = 0; i < fv; ++i) {
                    int vi = shape.mesh.indices[indexOffset + i].vertex_index;
                    smoothNormals[vi] += faceNormal;
                }

                indexOffset += fv;
            }
        }

        for (auto& n : smoothNormals) {
            float len = glm::length(n);
            if (len > 1e-8f) {
                n /= len;
            } else {
                n = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    auto fetchPos = [&](int vIdx) -> glm::vec3 {
        glm::vec3 v(attrib.vertices[3 * vIdx], attrib.vertices[3 * vIdx + 1], attrib.vertices[3 * vIdx + 2]);
        return rotY(v) * scale + offset;
    };
    auto fetchNormal = [&](int vIdx, int nIdx) -> glm::vec3 {
        if (hasNormals && nIdx >= 0) {
            return rotY(glm::vec3(attrib.normals[3 * nIdx], attrib.normals[3 * nIdx + 1], attrib.normals[3 * nIdx + 2]));
        }
        return rotY(smoothNormals[vIdx]);
    };

    // Keyed on (vertex, normal) so smoothing-group seams duplicate while shared vertices collapse.
    std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertexCache;

    auto resolveVertex = [&](const tinyobj::index_t& idx) -> uint32_t {
        VertexKey key{idx.vertex_index, hasNormals ? idx.normal_index : -1};
        auto      it = vertexCache.find(key);
        if (it != vertexCache.end()) {
            return it->second;
        }
        uint32_t newIdx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.emplace_back(fetchPos(idx.vertex_index), fetchNormal(idx.vertex_index, idx.normal_index));
        vertexCache.emplace(key, newIdx);
        return newIdx;
    };

    size_t totalTris = 0;
    for (const auto& shape : shapes) {
        for (int fv : shape.mesh.num_face_vertices) {
            if (fv >= 3) {
                totalTris += static_cast<size_t>(fv - 2);
            }
        }
    }
    mesh.indices.reserve(totalTris);

    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv < 3) {
                indexOffset += fv;
                continue;
            }

            uint32_t i0 = resolveVertex(shape.mesh.indices[indexOffset]);

            for (int i = 1; i + 1 < fv; ++i) {
                uint32_t i1 = resolveVertex(shape.mesh.indices[indexOffset + i]);
                uint32_t i2 = resolveVertex(shape.mesh.indices[indexOffset + i + 1]);
                mesh.indices.emplace_back(i0, i1, i2);
            }

            indexOffset += fv;
        }
    }

    if (mesh.indices.empty()) {
        return std::unexpected(std::format("OBJ '{}' contains no triangles", path.string()));
    }
    Log::info("Loaded OBJ: {} — {} triangles, {} unique vertices", path.filename().string(), mesh.indices.size(), mesh.vertices.size());
    return mesh;
}
