#include "obj_loader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include "log.h"

OBJMesh loadOBJ(const std::filesystem::path& path, uint32_t material_index, float scale, glm::vec3 offset, float rotateY) {
    OBJMesh mesh;

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.string().c_str())) {
        Log::error("Failed to load OBJ file: {} — {}", path.string(), err);
        return mesh;
    }
    if (!err.empty()) {
        Log::warn("OBJ: {}", err);
    }

    bool  hasNormals = !attrib.normals.empty();
    float cosY = cos(rotateY), sinY = sin(rotateY);
    auto  rotY = [&](glm::vec3 v) -> glm::vec3 {
        return glm::vec3(cosY * v.x + sinY * v.z, v.y, -sinY * v.x + cosY * v.z);
    };

    // If the OBJ has no vertex normals, compute smooth normals by averaging face normals at shared vertices
    std::vector<glm::vec3> smoothNormals;
    if (!hasNormals) {
        smoothNormals.resize(attrib.vertices.size() / 3, glm::vec3(0.0f));

        // Accumulate face normals at each vertex
        for (const auto& shape : shapes) {
            size_t indexOffset = 0;
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
                int fv = shape.mesh.num_face_vertices[f];

                auto      idx0 = shape.mesh.indices[indexOffset];
                auto      idx1 = shape.mesh.indices[indexOffset + 1];
                auto      idx2 = shape.mesh.indices[indexOffset + 2];
                glm::vec3 v0(
                    attrib.vertices[3 * idx0.vertex_index], attrib.vertices[3 * idx0.vertex_index + 1], attrib.vertices[3 * idx0.vertex_index + 2]);
                glm::vec3 v1(
                    attrib.vertices[3 * idx1.vertex_index], attrib.vertices[3 * idx1.vertex_index + 1], attrib.vertices[3 * idx1.vertex_index + 2]);
                glm::vec3 v2(
                    attrib.vertices[3 * idx2.vertex_index], attrib.vertices[3 * idx2.vertex_index + 1], attrib.vertices[3 * idx2.vertex_index + 2]);

                glm::vec3 faceNormal = glm::cross(v1 - v0, v2 - v0);
                // Weight by face area (unnormalized cross product magnitude = 2× area)
                // This naturally weights larger faces more

                // Add to all vertices of this face (including fan-triangulated verts)
                for (int i = 0; i < fv; ++i) {
                    int vi = shape.mesh.indices[indexOffset + i].vertex_index;
                    smoothNormals[vi] += faceNormal;
                }

                indexOffset += fv;
            }
        }

        // Normalize accumulated normals
        for (auto& n : smoothNormals) {
            float len = glm::length(n);
            if (len > 1e-8f) {
                n /= len;
            } else {
                n = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    // Count total triangles for reserve
    size_t totalTris = 0;
    for (const auto& shape : shapes) {
        totalTris += shape.mesh.num_face_vertices.size();
    }
    mesh.triangles.reserve(totalTris);

    // Build triangles with per-vertex normals
    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];

            auto      idx0 = shape.mesh.indices[indexOffset];
            glm::vec3 v0(
                attrib.vertices[3 * idx0.vertex_index], attrib.vertices[3 * idx0.vertex_index + 1], attrib.vertices[3 * idx0.vertex_index + 2]);
            v0 = rotY(v0) * scale + offset;

            glm::vec3 vn0;
            if (hasNormals && idx0.normal_index >= 0) {
                vn0 = rotY(glm::vec3(
                    attrib.normals[3 * idx0.normal_index], attrib.normals[3 * idx0.normal_index + 1], attrib.normals[3 * idx0.normal_index + 2]));
            } else {
                vn0 = rotY(smoothNormals[idx0.vertex_index]);
            }

            // Fan-triangulate
            for (int i = 1; i + 1 < fv; ++i) {
                auto idx1 = shape.mesh.indices[indexOffset + i];
                auto idx2 = shape.mesh.indices[indexOffset + i + 1];

                glm::vec3 v1(
                    attrib.vertices[3 * idx1.vertex_index], attrib.vertices[3 * idx1.vertex_index + 1], attrib.vertices[3 * idx1.vertex_index + 2]);
                glm::vec3 v2(
                    attrib.vertices[3 * idx2.vertex_index], attrib.vertices[3 * idx2.vertex_index + 1], attrib.vertices[3 * idx2.vertex_index + 2]);
                v1 = rotY(v1) * scale + offset;
                v2 = rotY(v2) * scale + offset;

                glm::vec3 vn1, vn2;
                if (hasNormals && idx1.normal_index >= 0) {
                    vn1 = rotY(glm::vec3(
                        attrib.normals[3 * idx1.normal_index], attrib.normals[3 * idx1.normal_index + 1], attrib.normals[3 * idx1.normal_index + 2]));
                } else {
                    vn1 = rotY(smoothNormals[idx1.vertex_index]);
                }

                if (hasNormals && idx2.normal_index >= 0) {
                    vn2 = rotY(glm::vec3(
                        attrib.normals[3 * idx2.normal_index], attrib.normals[3 * idx2.normal_index + 1], attrib.normals[3 * idx2.normal_index + 2]));
                } else {
                    vn2 = rotY(smoothNormals[idx2.vertex_index]);
                }

                mesh.triangles.emplace_back(v0, v1, v2, material_index, vn0, vn1, vn2);
            }

            indexOffset += fv;
        }
    }

    Log::info("Loaded OBJ: {} — {} triangles", path.filename().string(), mesh.triangles.size());
    return mesh;
}
