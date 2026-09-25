#include "scene/world.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <chrono>
#include <cmath>
#include <format>
#include <numbers>
#include <utility>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/log.h"
#include "core/alias_table.h"

uint32_t World::addMaterial(Material mat) {
    requireEditable("addMaterial");
    // Derived on insert so sortEmissiveFirst() and upload never see a stale class.
    mat.refreshType();
    materials.push_back(std::move(mat));
    return static_cast<uint32_t>(materials.size()) - 1;
}

uint32_t World::addVertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index) {
    requireEditable("addVertex");
    vertices.emplace_back(position, normal, material_index);
    return static_cast<uint32_t>(vertices.size()) - 1;
}

void World::addSphere(glm::vec3 center, float radius, uint32_t material_index, int latSegs, int lonSegs) {
    requireEditable("addSphere");
    const std::size_t firstTriangle = triangles.size();

    // Two spheres of one density are two copies; use addMeshAsset() + addObject() to share.
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
    requireEditable("addTriangle");
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
    requireEditable("addTriQuad");
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

void World::addMesh(const Mesh& mesh, uint32_t material_index) {
    requireEditable("addMesh");
    const std::size_t firstTriangle = triangles.size();
    const uint32_t    baseVertex = static_cast<uint32_t>(vertices.size());
    vertices.reserve(vertices.size() + mesh.vertices.size());
    for (const Vertex& v : mesh.vertices) {
        vertices.emplace_back(v.position, v.normal, material_index);
    }
    triangles.reserve(triangles.size() + mesh.indices.size());
    for (const glm::uvec3& tri : mesh.indices) {
        triangles.push_back(makeTriangle(vertices, baseVertex + tri.x, baseVertex + tri.y, baseVertex + tri.z, material_index));
    }
    recordImmediateObject(mesh.name.empty() ? "Mesh" : mesh.name, firstTriangle);
}

uint32_t World::addMeshAsset(Mesh mesh) {
    requireEditable("addMeshAsset");
    meshes.push_back(std::move(mesh));
    return static_cast<uint32_t>(meshes.size()) - 1;
}

uint32_t World::addObject(std::string name, uint32_t meshId, const glm::mat4& transform, uint32_t material_index) {
    requireEditable("addObject");
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
        // A mirroring transform reverses orientation; swapping two indices restores CCW about the outward normal.
        const bool flipWinding = glm::determinant(glm::mat3(o.transform)) < 0.0f;

        const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
        vertices.reserve(vertices.size() + mesh.vertices.size());
        // Per-vertex stamping lets two placements of one asset use two materials under the flat varying.
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
    if (created) {
        throw std::logic_error("World::create() called twice");
    }

    instantiateObjects();

    // Sort before validate() so it checks the order the GPU gets. Safe before refreshType(): isEmissive()
    // reads `emission`, not `type`.
    sortEmissiveFirst();

    if (auto valid = validate(); !valid) {
        throw std::runtime_error(std::format("Scene failed validation: {}", valid.error()));
    }
    // Catches materials mutated in place since addMaterial(); `type` decides queue routing.
    for (Material& m : materials) {
        m.refreshType();
    }
    buildLightGroups();

    const auto start = std::chrono::steady_clock::now();
    bvh.build(triangles, vertices);
    const std::chrono::duration<double, std::milli> duration = std::chrono::steady_clock::now() - start;
    Log::info("BVH Build time: {:.2f} ms", duration.count());

    created = true;
}

void World::requireEditable(std::string_view builder) const {
    if (created) {
        throw std::logic_error(std::format("World::{} called after create()", builder));
    }
}

namespace {
/// Layout shared by Triangle::alias_packed and LightGroup::aliasPacked.
uint32_t packAlias(float accept, uint32_t alias) {
    const auto q = static_cast<uint32_t>(std::lround(std::clamp(accept, 0.0f, 1.0f) * 65535.0f));
    return (q << 16) | (alias & 0xFFFFu);
}
} // namespace

void World::buildLightGroups() {
    lightGroups.clear();
    if (emissiveLastIndex < 0) {
        return;
    }
    const int end = emissiveLastIndex + 1;

    auto closeGroup = [&](int begin, int last) {
        const int count = last - begin + 1;
        assert(count <= 0xFFFF && "light group exceeds the 16-bit alias offset in Triangle::alias_packed");

        std::vector<float> areas(static_cast<std::size_t>(count));
        float              total = 0.0f;
        for (int i = 0; i < count; ++i) {
            areas[i] = triangles[begin + i].area;
            total += areas[i];
        }

        const AliasTable table = buildAliasTable(areas);
        for (int i = 0; i < count; ++i) {
            triangles[begin + i].alias_packed = packAlias(table.accept[i], table.alias[i]);
        }

        LightGroup g;
        g.begin = begin;
        g.count = count;
        g.totalArea = total;
        lightGroups.push_back(g);
    };

    // The alias target is a 16-bit offset from LightGroup::begin, so longer runs split into several groups.
    // Sound: groups are chosen against a stored pdf, so this shifts variance, never the mean.
    constexpr int MAX_GROUP_TRIANGLES = 0xFFFF;

    int      runBegin = 0;
    uint32_t runMat = triangles[0].material_index;
    for (int i = 1; i < end; ++i) {
        if (triangles[i].material_index != runMat || i - runBegin == MAX_GROUP_TRIANGLES) {
            closeGroup(runBegin, i - 1);
            runBegin = i;
            runMat = triangles[i].material_index;
        }
    }
    closeGroup(runBegin, end - 1);

    buildLightGroupSelection();

    Log::info("Light groups: {} (total {} emissive triangles)", lightGroups.size(), end);
}

void World::buildLightGroupSelection() {
    const std::size_t n = lightGroups.size();
    if (n == 0) {
        return;
    }

    // Power-weighted: uniform selection would give a dim fill light the key light's sample budget.
    std::vector<float> weight(n);
    double             total = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const glm::vec3 e = materials[triangles[lightGroups[i].begin].material_index].emittedRadiance();
        const float     luminance = 0.2126f * e.x + 0.7152f * e.y + 0.0722f * e.z;
        weight[i] = std::max(luminance, 0.0f) * lightGroups[i].totalArea;
        total += weight[i];
    }
    // Degenerate emitters must still form a valid distribution, or the pdf callers divide by is zero.
    if (!(total > 0.0)) {
        total = static_cast<double>(n);
        std::ranges::fill(weight, 1.0f);
    }

    const AliasTable table = buildAliasTable(weight);
    for (std::size_t i = 0; i < n; ++i) {
        lightGroups[i].aliasPacked = packAlias(table.accept[i], table.alias[i]);
        lightGroups[i].selectPdf = static_cast<float>(weight[i] / total);
    }
}

std::expected<void, std::string> World::validate() const {
    if (triangles.empty()) {
        return std::unexpected("no geometry");
    }
    if (materials.empty()) {
        return std::unexpected("no materials");
    }

    const uint32_t numMats = static_cast<uint32_t>(materials.size());
    const uint32_t numVerts = static_cast<uint32_t>(vertices.size());
    const auto     badMaterial = std::ranges::count_if(triangles, [&](const Triangle& t) { return t.material_index >= numMats; });
    const auto     badVertex = std::ranges::count_if(
        triangles, [&](const Triangle& t) { return t.indices.x >= numVerts || t.indices.y >= numVerts || t.indices.z >= numVerts; });
    if (badMaterial > 0 || badVertex > 0) {
        return std::unexpected(std::format("{} triangle(s) reference an out-of-range material, {} an out-of-range vertex", badMaterial, badVertex));
    }
    return {};
}

void World::sortEmissiveFirst() {
    const std::size_t n = triangles.size();
    // Triangles pushed without a builder have no owner, but the array must stay parallel.
    triangleObjectId.resize(n, NO_OBJECT);

    auto isEmissive = [&](const Triangle& t) {
        return t.material_index < materials.size() && materials[t.material_index].isEmissive();
    };

    // Explicit permutation: stable_partition on `triangles` alone can't carry `triangleObjectId` along.
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

    emissiveLastIndex = static_cast<int>(emissiveCount) - 1;
    Log::info("Emissive last index: {}", emissiveLastIndex);
}
