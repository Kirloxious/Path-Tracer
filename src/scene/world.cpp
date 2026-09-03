#include "scene/world.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <format>
#include <numbers>
#include <utility>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/log.h"

uint32_t World::addMaterial(Material mat) {
    // Derive the shading class on insert, before sortEmissiveFirst() or any upload can read
    // it. The static factories already do this; refreshing here covers materials assembled
    // field-by-field (which is what the scene editor will do).
    mat.refreshType();
    materials.push_back(std::move(mat));
    return static_cast<uint32_t>(materials.size()) - 1;
}

uint32_t World::addVertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index) {
    vertices.emplace_back(position, normal, material_index);
    return static_cast<uint32_t>(vertices.size()) - 1;
}

void World::addSphere(glm::vec3 center, float radius, uint32_t material_index, int latSegs, int lonSegs) {
    const std::size_t firstTriangle = triangles.size();

    // Two spheres of one density are two copies — addMeshAsset() + addObject() to share.
    const Mesh     mesh = makeUnitSphereMesh(latSegs, lonSegs);
    const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
    vertices.reserve(vertices.size() + mesh.vertices.size());
    for (const Vertex& v : mesh.vertices) {
        vertices.emplace_back(center + radius * v.position, v.normal, material_index);
    }
    triangles.reserve(triangles.size() + mesh.indices.size());
    for (const glm::uvec3& tri : mesh.indices) {
        triangles.push_back(makeTriangle(vertices, baseVertex + tri.x, baseVertex + tri.y, baseVertex + tri.z, material_index));
    }

    recordImmediateObject("Sphere", firstTriangle);
}

void World::addSphere(glm::vec3 center, float radius, Material mat, int latSegs, int lonSegs) {
    addSphere(center, radius, addMaterial(mat), latSegs, lonSegs);
}

void World::addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index) {
    const std::size_t firstTriangle = triangles.size();
    const glm::vec3   fn = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    const uint32_t    i0 = addVertex(v0, fn, material_index);
    const uint32_t    i1 = addVertex(v1, fn, material_index);
    const uint32_t    i2 = addVertex(v2, fn, material_index);
    triangles.push_back(makeTriangle(vertices, i0, i1, i2, material_index));
    recordImmediateObject("Triangle", firstTriangle);
}

void World::addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Material mat) {
    addTriangle(v0, v1, v2, addMaterial(mat));
}

void World::addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, uint32_t material_index) {
    const std::size_t firstTriangle = triangles.size();
    const glm::vec3   fn = glm::normalize(glm::cross(u, v));
    const uint32_t    ia = addVertex(corner, fn, material_index);
    const uint32_t    ib = addVertex(corner + u, fn, material_index);
    const uint32_t    ic = addVertex(corner + u + v, fn, material_index);
    const uint32_t    id = addVertex(corner + v, fn, material_index);
    triangles.push_back(makeTriangle(vertices, ia, ib, ic, material_index));
    triangles.push_back(makeTriangle(vertices, ia, ic, id, material_index));
    recordImmediateObject("Quad", firstTriangle);
}

void World::addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, Material mat) {
    addTriQuad(corner, u, v, addMaterial(mat));
}

void World::addMesh(const OBJMesh& mesh) {
    const std::size_t firstTriangle = triangles.size();
    const uint32_t    baseVertex = static_cast<uint32_t>(vertices.size());
    vertices.reserve(vertices.size() + mesh.vertices.size());
    for (const Vertex& v : mesh.vertices) {
        vertices.emplace_back(v.position, v.normal, mesh.material_index);
    }
    triangles.reserve(triangles.size() + mesh.indices.size());
    for (const glm::uvec3& tri : mesh.indices) {
        triangles.push_back(makeTriangle(vertices, baseVertex + tri.x, baseVertex + tri.y, baseVertex + tri.z, mesh.material_index));
    }
    recordImmediateObject(mesh.name.empty() ? "Mesh" : mesh.name, firstTriangle);
}

uint32_t World::addMeshAsset(Mesh mesh) {
    meshes.push_back(std::move(mesh));
    return static_cast<uint32_t>(meshes.size()) - 1;
}

uint32_t World::addMeshAsset(const OBJMesh& mesh) {
    return addMeshAsset(makeMeshFromOBJ(mesh));
}

uint32_t World::addObject(std::string name, uint32_t meshId, const glm::mat4& transform, uint32_t material_index) {
    Object o;
    o.name = std::move(name);
    o.meshId = meshId;
    o.transform = transform;
    o.material_index = material_index;
    objects.push_back(std::move(o));
    return static_cast<uint32_t>(objects.size()) - 1;
}

uint32_t World::addObject(std::string name, uint32_t meshId, const glm::mat4& transform, Material mat) {
    return addObject(std::move(name), meshId, transform, addMaterial(std::move(mat)));
}

uint32_t World::recordImmediateObject(std::string name, std::size_t firstTriangle) {
    Object o;
    o.name = std::format("{} {}", name, objects.size());
    o.meshId = NO_MESH;
    o.material_index = (firstTriangle < triangles.size()) ? triangles[firstTriangle].material_index : 0u;
    o.triangleCount = static_cast<uint32_t>(triangles.size() - firstTriangle);
    objects.push_back(std::move(o));

    const uint32_t objectId = static_cast<uint32_t>(objects.size()) - 1;
    // resize() only writes the slots the builder just added; earlier entries keep their owner.
    triangleObjectId.resize(triangles.size(), objectId);
    return objectId;
}

void World::instantiateObjects() {
    if (objectsInstantiated) {
        return;
    }
    objectsInstantiated = true;

    std::size_t placed = 0;
    for (std::size_t oi = 0; oi < objects.size(); ++oi) {
        Object& o = objects[oi];
        if (o.meshId == NO_MESH) {
            continue; // geometry already baked by an immediate-mode builder
        }
        if (o.meshId >= meshes.size()) {
            Log::error("Object '{}' references mesh {} but only {} are registered — skipped", o.name, o.meshId, meshes.size());
            continue;
        }
        const Mesh& mesh = meshes[o.meshId];
        if (mesh.empty()) {
            Log::warn("Object '{}' references empty mesh '{}' — skipped", o.name, mesh.name);
            continue;
        }

        const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(o.transform));
        // A mirroring transform reverses triangle orientation; swapping two indices restores
        // CCW-about-the-outward-normal, which NEE's light pdf and ReSTIR's target pdf need.
        const bool flipWinding = glm::determinant(glm::mat3(o.transform)) < 0.0f;

        const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
        vertices.reserve(vertices.size() + mesh.vertices.size());
        // Stamping the material per vertex is what lets two placements of one asset use two
        // materials without breaking the raster pass's flat material varying.
        for (const Vertex& v : mesh.vertices) {
            vertices.emplace_back(glm::vec3(o.transform * glm::vec4(v.position, 1.0f)), glm::normalize(normalMatrix * v.normal), o.material_index);
        }

        const std::size_t firstTriangle = triangles.size();
        triangles.reserve(triangles.size() + mesh.indices.size());
        for (const glm::uvec3& tri : mesh.indices) {
            uint32_t i1 = baseVertex + tri.y;
            uint32_t i2 = baseVertex + tri.z;
            if (flipWinding) {
                std::swap(i1, i2);
            }
            triangles.push_back(makeTriangle(vertices, baseVertex + tri.x, i1, i2, o.material_index));
        }

        o.triangleCount = static_cast<uint32_t>(triangles.size() - firstTriangle);
        triangleObjectId.resize(triangles.size(), static_cast<uint32_t>(oi));
        ++placed;
    }

    if (placed > 0) {
        Log::info("Instantiated {} object(s) from {} mesh asset(s)", placed, meshes.size());
    }
}

void World::create() {
    // Objects first: everything below needs their geometry to exist.
    instantiateObjects();

    // Then sort, so validate() checks the ordering the GPU will actually get. Safe before
    // refreshType(): Material::isEmissive() reads `emission`, not the cached `type`.
    sortEmissiveFirst();

    // A failed validate() means the geometry can't produce a usable BVH — bail before
    // bvh.build(), whose `assert(!triangles.empty())` is compiled out under NDEBUG and
    // whose `2 * n - 1` node count underflows to SIZE_MAX for n == 0.
    if (!validate()) {
        Log::error("World::create() aborted — scene failed validation");
        return;
    }
    // Idempotent: re-derive every cached MaterialClass so a material mutated in place since
    // addMaterial() (scene editor, or a factory assigning fields directly) cannot ship a stale
    // `type` to the GPU, where it decides queue routing.
    for (Material& m : materials) {
        m.refreshType();
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
    // create() sorts before validating, so this catches a broken sort, not a forgotten one.
    if (hasEmissive && emissiveLastIndex < 0) {
        Log::warn("World has emissive triangles but emissiveLastIndex={} — the emissive sort did not run", emissiveLastIndex);
    }

    return true;
}

void World::sortEmissiveFirst() {
    const std::size_t n = triangles.size();
    // Triangles pushed in without a builder have no owner; the array must still be parallel
    // or the permutation below reads past its end.
    triangleObjectId.resize(n, NO_OBJECT);

    auto isEmissive = [&](const Triangle& t) {
        return t.material_index < materials.size() && materials[t.material_index].isEmissive();
    };

    // Explicit permutation rather than stable_partition on `triangles`: partitioning one
    // array cannot carry `triangleObjectId` along. Index order keeps it stable.
    std::vector<uint32_t> order;
    order.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        if (isEmissive(triangles[i])) {
            order.push_back(i);
        }
    }
    const std::size_t emissiveCount = order.size();
    for (uint32_t i = 0; i < n; ++i) {
        if (!isEmissive(triangles[i])) {
            order.push_back(i);
        }
    }

    std::vector<Triangle> sortedTriangles;
    std::vector<uint32_t> sortedOwners;
    sortedTriangles.reserve(n);
    sortedOwners.reserve(n);
    for (const uint32_t i : order) {
        sortedTriangles.push_back(triangles[i]);
        sortedOwners.push_back(triangleObjectId[i]);
    }
    triangles = std::move(sortedTriangles);
    triangleObjectId = std::move(sortedOwners);

    // -1 when there are no emissive triangles at all.
    emissiveLastIndex = static_cast<int>(emissiveCount) - 1;
    Log::info("Emissive last index: {}", emissiveLastIndex);
}
