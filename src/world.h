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

        auto start = std::chrono::high_resolution_clock::now();
        bvh.build(spheres, triangles);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> duration = end - start;
        Log::info("BVH Build time: {:.2f} ms", duration.count());
    }

    int sortEmissiveFirst() {
        auto it =
            std::stable_partition(spheres.begin(), spheres.end(), [&](const Sphere& s) { return (int)materials[s.material_index].isEmissive(); });
        return (int)std::distance(spheres.begin(), it) - 1; // last emissive index
    }
};
