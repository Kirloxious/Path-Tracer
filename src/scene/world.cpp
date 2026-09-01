#include "scene/world.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <numbers>
#include <utility>

#include "core/log.h"

uint32_t World::addMaterial(Material mat) {
    materials.push_back(std::move(mat));
    return static_cast<uint32_t>(materials.size()) - 1;
}

uint32_t World::addVertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index) {
    vertices.emplace_back(position, normal, material_index);
    return static_cast<uint32_t>(vertices.size()) - 1;
}

void World::addSphere(glm::vec3 center, float radius, uint32_t material_index, int latSegs, int lonSegs) {
    constexpr float PI = std::numbers::pi_v<float>;
    const int       LAT = std::max(latSegs, 2);
    const int       LON = std::max(lonSegs, 3);

    // (LAT + 1) rows × LON columns of vertices; columns wrap (col == LON ≡ col == 0).
    const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
    for (int lat = 0; lat <= LAT; ++lat) {
        const float phi = static_cast<float>(lat) / static_cast<float>(LAT) * PI;
        const float sphi = std::sin(phi);
        const float cphi = std::cos(phi);
        for (int lon = 0; lon < LON; ++lon) {
            const float     th = static_cast<float>(lon) / static_cast<float>(LON) * 2.0f * PI;
            const glm::vec3 n{sphi * std::cos(th), cphi, sphi * std::sin(th)};
            vertices.emplace_back(center + radius * n, n, material_index);
        }
    }

    auto vIdx = [&](int lat, int lon) -> uint32_t {
        return baseVertex + static_cast<uint32_t>(lat * LON + (lon % LON));
    };

    for (int lat = 0; lat < LAT; ++lat) {
        for (int lon = 0; lon < LON; ++lon) {
            const uint32_t a = vIdx(lat, lon);
            const uint32_t b = vIdx(lat + 1, lon);
            const uint32_t c = vIdx(lat + 1, lon + 1);
            const uint32_t d = vIdx(lat, lon + 1);

            // Wind CCW relative to the outward (vertex) normal so cross(e1, e2)
            // is outward. The NEE light-pdf code and ReSTIR's target-pdf both
            // gate on `dot(cross(e1, e2), light_dir) < 0` to detect receiver-
            // facing light samples; CW winding silently rejects every sphere-
            // light sample and forces all sphere-light direct illumination
            // through BSDF-hits-emissive only.
            if (lat != 0) {
                triangles.push_back(makeTriangle(vertices, a, d, c, material_index));
            }
            if (lat != LAT - 1) {
                triangles.push_back(makeTriangle(vertices, a, c, b, material_index));
            }
        }
    }
}

void World::addSphere(glm::vec3 center, float radius, Material mat, int latSegs, int lonSegs) {
    addSphere(center, radius, addMaterial(mat), latSegs, lonSegs);
}

void World::addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index) {
    const glm::vec3 fn = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    const uint32_t  i0 = addVertex(v0, fn, material_index);
    const uint32_t  i1 = addVertex(v1, fn, material_index);
    const uint32_t  i2 = addVertex(v2, fn, material_index);
    triangles.push_back(makeTriangle(vertices, i0, i1, i2, material_index));
}

void World::addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Material mat) {
    addTriangle(v0, v1, v2, addMaterial(mat));
}

void World::addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, uint32_t material_index) {
    const glm::vec3 fn = glm::normalize(glm::cross(u, v));
    const uint32_t  ia = addVertex(corner, fn, material_index);
    const uint32_t  ib = addVertex(corner + u, fn, material_index);
    const uint32_t  ic = addVertex(corner + u + v, fn, material_index);
    const uint32_t  id = addVertex(corner + v, fn, material_index);
    triangles.push_back(makeTriangle(vertices, ia, ib, ic, material_index));
    triangles.push_back(makeTriangle(vertices, ia, ic, id, material_index));
}

void World::addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, Material mat) {
    addTriQuad(corner, u, v, addMaterial(mat));
}

void World::addMesh(const OBJMesh& mesh) {
    const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
    vertices.reserve(vertices.size() + mesh.vertices.size());
    for (const Vertex& v : mesh.vertices) {
        vertices.emplace_back(v.position, v.normal, mesh.material_index);
    }
    triangles.reserve(triangles.size() + mesh.indices.size());
    for (const glm::uvec3& tri : mesh.indices) {
        triangles.push_back(makeTriangle(vertices, baseVertex + tri.x, baseVertex + tri.y, baseVertex + tri.z, mesh.material_index));
    }
}

void World::create() {
    // A failed validate() means the geometry can't produce a usable BVH — bail before
    // bvh.build(), whose `assert(!triangles.empty())` is compiled out under NDEBUG and
    // whose `2 * n - 1` node count underflows to SIZE_MAX for n == 0.
    if (!validate()) {
        Log::error("World::create() aborted — scene failed validation");
        return;
    }
    buildLightGroups();

    const auto start = std::chrono::high_resolution_clock::now();
    bvh.build(triangles, vertices);
    const auto end = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double, std::milli> duration = end - start;
    Log::info("BVH Build time: {:.2f} ms", duration.count());
}

void World::buildLightGroups() {
    lightGroups.clear();
    if (emissiveLastIndex < 0) {
        return;
    }
    const int end = emissiveLastIndex + 1;

    // Vose alias-table construction over the group's triangles, weighted by area. Sampling
    // is then: draw a uniform slot, accept it with `prob`, otherwise take its alias — O(1),
    // one load, versus the log2(count) chain of dependent scattered loads a CDF search cost.
    auto closeGroup = [&](int begin, int last) {
        const int count = last - begin + 1;
        assert(count <= 0xFFFF && "light group exceeds the 16-bit alias offset in Triangle::alias_packed");

        float total = 0.0f;
        for (int i = begin; i <= last; ++i) {
            total += triangles[i].area;
        }

        // Scaled probabilities: p[i] = count * area[i] / total, so the mean is exactly 1
        // and each slot is either under- or over-full.
        std::vector<float> p(static_cast<std::size_t>(count));
        std::vector<int>   alias(static_cast<std::size_t>(count), 0);
        std::vector<float> prob(static_cast<std::size_t>(count), 1.0f);
        std::vector<int>   small;
        std::vector<int>   large;
        small.reserve(count);
        large.reserve(count);

        for (int i = 0; i < count; ++i) {
            p[i] = (total > 0.0f) ? (static_cast<float>(count) * triangles[begin + i].area / total) : 1.0f;
            (p[i] < 1.0f ? small : large).push_back(i);
        }

        // Pair each under-full slot with an over-full one until one list empties.
        while (!small.empty() && !large.empty()) {
            const int l = small.back();
            small.pop_back();
            const int g = large.back();
            large.pop_back();

            prob[l] = p[l];
            alias[l] = g;
            p[g] = (p[g] + p[l]) - 1.0f;
            (p[g] < 1.0f ? small : large).push_back(g);
        }
        // Whatever remains is full to within rounding; accept it unconditionally.
        for (const int i : large) {
            prob[i] = 1.0f;
        }
        for (const int i : small) {
            prob[i] = 1.0f;
        }

        for (int i = 0; i < count; ++i) {
            const uint32_t q = static_cast<uint32_t>(std::lround(std::clamp(prob[i], 0.0f, 1.0f) * 65535.0f));
            triangles[begin + i].alias_packed = (q << 16) | (static_cast<uint32_t>(alias[i]) & 0xFFFFu);
        }

        LightGroup g;
        g.begin = begin;
        g.count = count;
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

bool World::validate() const {
    if (triangles.empty()) {
        Log::error("World::create() called with no geometry — BVH will be empty");
        return false;
    }
    if (materials.empty()) {
        Log::error("World has no materials — every triangle's material_index is invalid");
        return false;
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

    const bool hasEmissive = std::any_of(triangles.begin(), triangles.end(), [&](const Triangle& t) {
        return t.material_index < numMats && materials[t.material_index].isEmissive();
    });
    if (hasEmissive && emissiveLastIndex < 0) {
        Log::warn("World has emissive triangles but emissiveLastIndex={} — did you forget sortEmissiveFirst()?", emissiveLastIndex);
    }

    return true;
}

void World::sortEmissiveFirst() {
    const auto it =
        std::stable_partition(triangles.begin(), triangles.end(), [&](const Triangle& t) { return materials[t.material_index].isEmissive(); });
    // -1 when there are no emissive triangles at all.
    emissiveLastIndex = static_cast<int>(std::distance(triangles.begin(), it)) - 1;
    Log::info("Emissive last index: {}", emissiveLastIndex);
}
