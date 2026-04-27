#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

#include "bvh.h"
#include "log.h"
#include "material.h"
#include "obj_loader.h"

class World
{
public:
    // (begin, count, total_area) — one entry per source emissive primitive. Stored as a
    // 16-byte struct so std430 upload matches the GLSL declaration exactly.
    struct alignas(16) LightGroup
    {
        int32_t begin = 0;
        int32_t count = 0;
        float   total_area = 0.0f;
        float   _pad = 0.0f;
    };

    std::vector<Vertex>     vertices;
    std::vector<Triangle>   triangles;
    std::vector<Material>   materials;
    std::vector<LightGroup> lightGroups;
    BVH                     bvh;
    int                     emissiveLastIndex = -1;

    uint32_t addMaterial(Material mat) {
        materials.push_back(mat);
        return (uint32_t)materials.size() - 1;
    }

    uint32_t addVertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index) {
        vertices.emplace_back(position, normal, material_index);
        return (uint32_t)vertices.size() - 1;
    }

    // Spheres are tessellated into triangles immediately. Smooth analytic per-vertex
    // normals; pole rows emit one triangle per longitude (the other would be degenerate).
    //
    // Default density (16 lon x 8 lat = 224 tris) is a compromise between silhouette
    // quality and BVH cost. Tiny / barely-visible spheres should pass a lower density —
    // every triangle ends up in the scene BVH.
    void addSphere(glm::vec3 center, float radius, uint32_t material_index, int latSegs = 8, int lonSegs = 16) {
        constexpr float PI = 3.14159265358979323846f;
        const int       LAT = std::max(latSegs, 2);
        const int       LON = std::max(lonSegs, 3);

        // (LAT + 1) rows × LON columns of vertices; columns wrap (col == LON ≡ col == 0).
        const uint32_t baseVertex = (uint32_t)vertices.size();
        for (int lat = 0; lat <= LAT; ++lat) {
            float phi = float(lat) / float(LAT) * PI;
            float sphi = std::sin(phi), cphi = std::cos(phi);
            for (int lon = 0; lon < LON; ++lon) {
                float     th = float(lon) / float(LON) * 2.0f * PI;
                glm::vec3 n{sphi * std::cos(th), cphi, sphi * std::sin(th)};
                vertices.emplace_back(center + radius * n, n, material_index);
            }
        }

        auto vIdx = [&](int lat, int lon) -> uint32_t {
            return baseVertex + uint32_t(lat * LON + (lon % LON));
        };

        for (int lat = 0; lat < LAT; ++lat) {
            for (int lon = 0; lon < LON; ++lon) {
                uint32_t a = vIdx(lat, lon);
                uint32_t b = vIdx(lat + 1, lon);
                uint32_t c = vIdx(lat + 1, lon + 1);
                uint32_t d = vIdx(lat, lon + 1);

                if (lat != 0) {
                    triangles.push_back(makeTriangle(vertices, a, c, d, material_index));
                }
                if (lat != LAT - 1) {
                    triangles.push_back(makeTriangle(vertices, a, b, c, material_index));
                }
            }
        }
    }

    void addSphere(glm::vec3 center, float radius, Material mat, int latSegs = 8, int lonSegs = 16) {
        addSphere(center, radius, addMaterial(mat), latSegs, lonSegs);
    }

    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index) {
        glm::vec3 fn = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        uint32_t  i0 = addVertex(v0, fn, material_index);
        uint32_t  i1 = addVertex(v1, fn, material_index);
        uint32_t  i2 = addVertex(v2, fn, material_index);
        triangles.push_back(makeTriangle(vertices, i0, i1, i2, material_index));
    }

    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Material mat) { addTriangle(v0, v1, v2, addMaterial(mat)); }

    void addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, uint32_t material_index) {
        glm::vec3 fn = glm::normalize(glm::cross(u, v));
        uint32_t  ia = addVertex(corner, fn, material_index);
        uint32_t  ib = addVertex(corner + u, fn, material_index);
        uint32_t  ic = addVertex(corner + u + v, fn, material_index);
        uint32_t  id = addVertex(corner + v, fn, material_index);
        triangles.push_back(makeTriangle(vertices, ia, ib, ic, material_index));
        triangles.push_back(makeTriangle(vertices, ia, ic, id, material_index));
    }

    void addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, Material mat) { addTriQuad(corner, u, v, addMaterial(mat)); }

    void addMesh(const OBJMesh& mesh) {
        const uint32_t baseVertex = (uint32_t)vertices.size();
        vertices.reserve(vertices.size() + mesh.vertices.size());
        for (const Vertex& v : mesh.vertices) {
            vertices.emplace_back(v.position, v.normal, mesh.material_index);
        }
        triangles.reserve(triangles.size() + mesh.indices.size());
        for (const glm::uvec3& tri : mesh.indices) {
            triangles.push_back(makeTriangle(vertices, baseVertex + tri.x, baseVertex + tri.y, baseVertex + tri.z, mesh.material_index));
        }
    }

    void create() {

        validate();
        buildLightGroups();

        auto start = std::chrono::high_resolution_clock::now();
        bvh.build(triangles, vertices);
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
    // Group consecutive emissive triangles that share a material into a single light, then
    // bake an area-weighted CDF into each emissive triangle's `cdf_in_group`. NEE samples
    // a group, then binary-searches the CDF to pick a triangle proportional to area —
    // making a tessellated sphere/quad behave like one uniform area light, regardless of
    // how the triangles are sized (poles vs. equator on a sphere).
    void buildLightGroups() {
        lightGroups.clear();
        if (emissiveLastIndex < 0) {
            return;
        }
        const int end = emissiveLastIndex + 1;

        auto closeGroup = [&](int begin, int last) {
            float total = 0.0f;
            for (int i = begin; i <= last; ++i) {
                total += triangles[i].area;
            }
            float cumulative = 0.0f;
            for (int i = begin; i <= last; ++i) {
                cumulative += triangles[i].area;
                triangles[i].cdf_in_group = (total > 0.0f) ? (cumulative / total) : 0.0f;
            }
            // Anchor the last entry so a binary search with r in [0, 1) always lands.
            triangles[last].cdf_in_group = 1.0f;

            LightGroup g;
            g.begin = begin;
            g.count = last - begin + 1;
            g.total_area = total;
            lightGroups.push_back(g);
        };

        int      runBegin = 0;
        uint32_t runMat = triangles[0].material_index;
        for (int i = 1; i < end; ++i) {
            if (triangles[i].material_index != runMat) {
                closeGroup(runBegin, i - 1);
                runBegin = i;
                runMat = triangles[i].material_index;
            }
        }
        closeGroup(runBegin, end - 1);

        Log::info("Light groups: {} (total {} emissive triangles)", lightGroups.size(), end);
    }

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
        const uint32_t numVerts = static_cast<uint32_t>(vertices.size());
        size_t         badTris = 0;
        size_t         badIndices = 0;
        for (const auto& t : triangles) {
            if (t.material_index >= numMats) {
                ++badTris;
            }
            if (t.indices.x >= numVerts || t.indices.y >= numVerts || t.indices.z >= numVerts) {
                ++badIndices;
            }
        }
        if (badTris > 0) {
            Log::error("{} triangle(s) reference out-of-range material_index (max = {})", badTris, numMats - 1);
        }
        if (badIndices > 0) {
            Log::error("{} triangle(s) reference out-of-range vertex index (max = {})", badIndices, numVerts - 1);
        }

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
