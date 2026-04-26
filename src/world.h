#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
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
    std::vector<Triangle>   triangles;
    std::vector<Material>   materials;
    std::vector<glm::ivec2> lightGroups; // (begin, count) — contiguous runs of emissive triangles sharing a source primitive
    BVH                     bvh;
    int                     emissiveLastIndex = -1;

    // Returns the index of the added material
    uint32_t addMaterial(Material mat) {
        materials.push_back(mat);
        return (uint32_t)materials.size() - 1;
    }

    // Spheres are tessellated into triangles immediately. Smooth analytic per-vertex
    // normals; pole rows emit one triangle per longitude (the other would be degenerate).
    // The path tracer only sees triangles after this call returns.
    //
    // Default density (16 lon x 8 lat = 224 tris) is a compromise between silhouette
    // quality and BVH cost. Tiny / barely-visible spheres should pass a lower density —
    // every triangle ends up in the scene BVH.
    void addSphere(glm::vec3 center, float radius, uint32_t material_index, int latSegs = 8, int lonSegs = 16) {
        constexpr float PI = 3.14159265358979323846f;
        const int       LAT = std::max(latSegs, 2);
        const int       LON = std::max(lonSegs, 3);

        auto vertex = [&](float theta, float phi) {
            glm::vec3 n{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
            return std::pair<glm::vec3, glm::vec3>{center + radius * n, n};
        };

        for (int lat = 0; lat < LAT; ++lat) {
            float phi0 = float(lat) / float(LAT) * PI;
            float phi1 = float(lat + 1) / float(LAT) * PI;

            for (int lon = 0; lon < LON; ++lon) {
                float th0 = float(lon) / float(LON) * 2.0f * PI;
                float th1 = float(lon + 1) / float(LON) * 2.0f * PI;

                auto [pa, na] = vertex(th0, phi0);
                auto [pb, nb] = vertex(th0, phi1);
                auto [pc, nc] = vertex(th1, phi1);
                auto [pd, nd] = vertex(th1, phi0);

                if (lat != 0) {
                    triangles.emplace_back(pa, pc, pd, material_index, na, nc, nd);
                }
                if (lat != LAT - 1) {
                    triangles.emplace_back(pa, pb, pc, material_index, na, nb, nc);
                }
            }
        }
    }

    void addSphere(glm::vec3 center, float radius, Material mat, int latSegs = 8, int lonSegs = 16) {
        addSphere(center, radius, addMaterial(mat), latSegs, lonSegs);
    }

    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index) { triangles.emplace_back(v0, v1, v2, material_index); }

    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Material mat) { addTriangle(v0, v1, v2, addMaterial(mat)); }

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
        buildLightGroups();

        auto start = std::chrono::high_resolution_clock::now();
        bvh.build(triangles);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> duration = end - start;
        Log::info("BVH Build time: {:.2f} ms", duration.count());
    }

    // Group consecutive emissive triangles that share a material into a single "light group".
    // This makes NEE pick a *light* uniformly first, then a triangle within it — variance
    // for tessellated emissive spheres becomes comparable to sampling them analytically.
    // Relies on (a) sortEmissiveFirst() having been called by the scene factory and (b)
    // every emissive source being added with a single material — true for addSphere() and
    // addTriQuad(); a scene that wants two distinct lights with the same material should
    // duplicate the material.
    void buildLightGroups() {
        lightGroups.clear();
        if (emissiveLastIndex < 0) {
            return;
        }
        const int end = emissiveLastIndex + 1;
        int       runBegin = 0;
        uint32_t  runMat = triangles[0].material_index;
        for (int i = 1; i < end; ++i) {
            if (triangles[i].material_index != runMat) {
                lightGroups.emplace_back(runBegin, i - runBegin);
                runBegin = i;
                runMat = triangles[i].material_index;
            }
        }
        lightGroups.emplace_back(runBegin, end - runBegin);
        Log::info("Light groups: {} (total {} emissive triangles)", lightGroups.size(), end);
    }

    // Catches the most common silent failure modes before they reach the GPU.
    void validate() const {
        if (triangles.empty()) {
            Log::error("World::create() called with no geometry — BVH will be empty");
            return;
        }
        if (materials.empty()) {
            Log::error("World has no materials — every triangle's material_index is invalid");
            return;
        }

        const uint32_t numMats = static_cast<uint32_t>(materials.size());
        size_t         badTris = 0;
        for (const auto& t : triangles) {
            if (t.material_index >= numMats) {
                ++badTris;
            }
        }
        if (badTris > 0) {
            Log::error("{} triangle(s) reference out-of-range material_index (max = {})", badTris, numMats - 1);
        }

        // NEE walks triangles[0 .. emissiveLastIndex]. If any emissive triangle exists but the
        // first triangle isn't emissive, sortEmissiveFirst() was never called.
        bool hasEmissive = false;
        for (const auto& t : triangles) {
            if (t.material_index < numMats && materials[t.material_index].isEmissive()) {
                hasEmissive = true;
                break;
            }
        }
        if (hasEmissive && emissiveLastIndex < 0) {
            Log::warn("World has emissive triangles but emissiveLastIndex={} — did you forget sortEmissiveFirst()?", emissiveLastIndex);
        }
    }

    int sortEmissiveFirst() {
        auto it =
            std::stable_partition(triangles.begin(), triangles.end(), [&](const Triangle& t) { return materials[t.material_index].isEmissive(); });
        return (int)std::distance(triangles.begin(), it) - 1; // last emissive index, -1 if none
    }
};
