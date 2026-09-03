#include "scene/mesh.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <utility>

Mesh makeUnitSphereMesh(int latSegs, int lonSegs) {
    constexpr float PI = std::numbers::pi_v<float>;
    const int       LAT = std::max(latSegs, 2);
    const int       LON = std::max(lonSegs, 3);

    Mesh mesh;
    mesh.name = std::format("Sphere {}x{}", LAT, LON);

    // (LAT + 1) rows x LON columns of vertices; columns wrap (col == LON is col == 0).
    mesh.vertices.reserve(static_cast<std::size_t>(LAT + 1) * static_cast<std::size_t>(LON));
    for (int lat = 0; lat <= LAT; ++lat) {
        const float phi = static_cast<float>(lat) / static_cast<float>(LAT) * PI;
        const float sphi = std::sin(phi);
        const float cphi = std::cos(phi);
        for (int lon = 0; lon < LON; ++lon) {
            const float     th = static_cast<float>(lon) / static_cast<float>(LON) * 2.0f * PI;
            const glm::vec3 n{sphi * std::cos(th), cphi, sphi * std::sin(th)};
            mesh.vertices.emplace_back(n, n);
        }
    }

    auto vIdx = [&](int lat, int lon) -> uint32_t {
        return static_cast<uint32_t>(lat * LON + (lon % LON));
    };

    mesh.indices.reserve(static_cast<std::size_t>(2 * LAT - 2) * static_cast<std::size_t>(LON));
    for (int lat = 0; lat < LAT; ++lat) {
        for (int lon = 0; lon < LON; ++lon) {
            const uint32_t a = vIdx(lat, lon);
            const uint32_t b = vIdx(lat + 1, lon);
            const uint32_t c = vIdx(lat + 1, lon + 1);
            const uint32_t d = vIdx(lat, lon + 1);

            // NEE's light pdf and ReSTIR's target pdf both gate on
            // `dot(cross(e1, e2), light_dir) < 0`, so CW winding here silently rejects every
            // sphere-light sample and leaves only BSDF-hits-emissive to light the scene.
            if (lat != 0) {
                mesh.indices.emplace_back(a, d, c);
            }
            if (lat != LAT - 1) {
                mesh.indices.emplace_back(a, c, b);
            }
        }
    }

    return mesh;
}

Mesh makeQuadMesh(glm::vec3 u, glm::vec3 v, std::string name) {
    const glm::vec3 fn = glm::normalize(glm::cross(u, v));

    Mesh mesh;
    mesh.name = std::move(name);
    mesh.vertices = {
        Vertex(glm::vec3(0.0f), fn),
        Vertex(u, fn),
        Vertex(u + v, fn),
        Vertex(v, fn),
    };
    mesh.indices = {glm::uvec3(0, 1, 2), glm::uvec3(0, 2, 3)};
    return mesh;
}

Mesh makeTriangleMesh(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, std::string name) {
    const glm::vec3 fn = glm::normalize(glm::cross(v1 - v0, v2 - v0));

    Mesh mesh;
    mesh.name = std::move(name);
    mesh.vertices = {Vertex(v0, fn), Vertex(v1, fn), Vertex(v2, fn)};
    mesh.indices = {glm::uvec3(0, 1, 2)};
    return mesh;
}

Mesh makeMeshFromOBJ(const OBJMesh& objMesh) {
    Mesh mesh;
    mesh.name = objMesh.name;
    mesh.vertices = objMesh.vertices;
    mesh.indices = objMesh.indices;
    // The Object supplies the material, so one asset can be placed under several.
    for (Vertex& vert : mesh.vertices) {
        vert.material_index = 0;
    }
    return mesh;
}
