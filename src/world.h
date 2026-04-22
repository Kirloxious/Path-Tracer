#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <glm/glm.hpp>
#include <ratio>

#include "bvh.h"
#include "log.h"
#include "material.h"
#include "obj_loader.h"

class World
{
public:
    std::vector<Sphere>   spheres;
    std::vector<Triangle> triangles;
    std::vector<Quad>     quads;
    std::vector<Material> materials;
    BVH                   bvh;
    int                   emissiveLastIndex = 0;

    // Returns the index of the added material
    uint32_t addMaterial(Material mat) {
        materials.push_back(mat);
        return (uint32_t)materials.size() - 1;
    }

    void addSphere(glm::vec3 position, float radius, uint32_t material_index) { spheres.emplace_back(position, radius, material_index); }

    void addSphere(glm::vec3 position, float radius, Material mat) { addSphere(position, radius, addMaterial(mat)); }

    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index) { triangles.emplace_back(v0, v1, v2, material_index); }

    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Material mat) { addTriangle(v0, v1, v2, addMaterial(mat)); }

    void addQuad(glm::vec3 corner_point, glm::vec3 u, glm::vec3 v, uint32_t material_index) {
        quads.emplace_back(corner_point, u, v, material_index);
    }

    void addQuad(glm::vec3 corner_point, glm::vec3 u, glm::vec3 v, Material mat) { quads.emplace_back(corner_point, u, v, addMaterial(mat)); }

    // Adds a quad as two triangles (for BVH traversal which only handles spheres + triangles)
    void addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, uint32_t material_index) {
        glm::vec3 a = corner;
        glm::vec3 b = corner + u;
        glm::vec3 c = corner + u + v;
        glm::vec3 d = corner + v;
        addTriangle(a, b, c, material_index);
        addTriangle(a, c, d, material_index);
    }

    void addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, Material mat) { addTriQuad(corner, u, v, addMaterial(mat)); }

    void addMesh(const OBJMesh& mesh) { triangles.insert(triangles.end(), mesh.triangles.begin(), mesh.triangles.end()); }

    void create() {

        validate();

        auto start = std::chrono::high_resolution_clock::now();
        bvh.build(spheres, triangles);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> duration = end - start;
        Log::info("BVH Build time: {:.2f} ms", duration.count());
    }

    // Catches the most common silent failure modes before they reach the GPU:
    // empty scenes, dangling material indices, or forgetting to call sortEmissiveFirst().
    void validate() const {
        if (spheres.empty() && triangles.empty()) {
            Log::error("World::create() called with no geometry — BVH will be empty");
            return;
        }
        if (materials.empty()) {
            Log::error("World has no materials — every primitive's material_index is invalid");
            return;
        }

        const uint32_t numMats = static_cast<uint32_t>(materials.size());
        size_t         badSpheres = 0;
        size_t         badTris = 0;
        for (const auto& s : spheres) {
            if (s.material_index >= numMats) {
                ++badSpheres;
            }
        }
        for (const auto& t : triangles) {
            if (t.material_index >= numMats) {
                ++badTris;
            }
        }
        if (badSpheres > 0) {
            Log::error("{} sphere(s) reference out-of-range material_index (max = {})", badSpheres, numMats - 1);
        }
        if (badTris > 0) {
            Log::error("{} triangle(s) reference out-of-range material_index (max = {})", badTris, numMats - 1);
        }

        // NEE light sampling walks spheres[0 .. emissiveLastIndex]. If any emissive sphere exists
        // but emissiveLastIndex is 0 (the default), sortEmissiveFirst() was never called and
        // direct-light sampling will silently miss lights or sample non-emissive spheres as lights.
        bool hasEmissive = false;
        for (const auto& s : spheres) {
            if (s.material_index < numMats && materials[s.material_index].isEmissive()) {
                hasEmissive = true;
                break;
            }
        }
        if (hasEmissive && emissiveLastIndex <= 0 && !spheres.empty() && !materials[spheres[0].material_index].isEmissive()) {
            Log::warn("World has emissive spheres but emissiveLastIndex={} — did you forget sortEmissiveFirst()?", emissiveLastIndex);
        }
    }

    int sortEmissiveFirst() {
        auto it =
            std::stable_partition(spheres.begin(), spheres.end(), [&](const Sphere& s) { return (int)materials[s.material_index].isEmissive(); });
        return (int)std::distance(spheres.begin(), it) - 1; // last emissive index
    }
};
