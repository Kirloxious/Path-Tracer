#include "scene/scene.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "core/log.h"
#include "scene/obj_loader.h"
#include "core/utils.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {
// A scene whose asset is missing fails as a whole rather than rendering without it.
Mesh requireOBJ(const std::filesystem::path& path, float scale = 1.0f, glm::vec3 offset = glm::vec3(0.0f), float rotateY = 0.0f) {
    auto mesh = loadOBJ(path, scale, offset, rotateY);
    if (!mesh) {
        throw std::runtime_error(mesh.error());
    }
    return std::move(*mesh);
}
} // namespace

Scene Scene::CornellBox() {
    Scene scene;
    scene.name = "Cornell Box";
    Log::info("Building scene: {}", scene.name);

    scene.cameraSettings.aspect_ratio = 16.0f / 9.0f;
    scene.cameraSettings.image_width = 1200;
    scene.cameraSettings.max_bounces = 32;
    scene.cameraSettings.vfov = 40.0f;
    scene.cameraSettings.lookfrom = glm::vec3(27.75f, 27.75f, -75.0f);
    scene.cameraSettings.lookat = glm::vec3(27.75f, 27.75f, 27.75f);

    World& w = scene.world;

    constexpr float S = 55.5f;

    uint32_t white = w.addMaterial(Material::Lambertian(glm::vec3(0.73f, 0.73f, 0.73f)));
    uint32_t red = w.addMaterial(Material::Lambertian(glm::vec3(0.65f, 0.05f, 0.05f)));
    uint32_t green = w.addMaterial(Material::Lambertian(glm::vec3(0.12f, 0.45f, 0.15f)));

    w.addSphere(glm::vec3(S * 0.5f, S * 0.93f, S * 0.5f), S * 0.06f, Material::Emissive(glm::vec3(1.0f), glm::vec3(8.0f)), 16, 32);

    // Open front at z = 0.
    w.addTriQuad(glm::vec3(S, 0, 0), glm::vec3(0, 0, S), glm::vec3(0, S, 0), red);   // Left (red)
    w.addTriQuad(glm::vec3(0, 0, 0), glm::vec3(0, S, 0), glm::vec3(0, 0, S), green); // Right (green)
    w.addTriQuad(glm::vec3(0, 0, 0), glm::vec3(0, 0, S), glm::vec3(S, 0, 0), white); // Floor
    w.addTriQuad(glm::vec3(0, S, 0), glm::vec3(S, 0, 0), glm::vec3(0, 0, S), white); // Ceiling
    w.addTriQuad(glm::vec3(0, 0, S), glm::vec3(0, S, 0), glm::vec3(S, 0, 0), white); // Back wall

    float     angle = glm::radians(15.0f);
    float     cs = cos(angle), sn = sin(angle);
    float     tallW = S * 0.297f, tallH = S * 0.595f;
    glm::vec3 tallCenter(S * 0.663f, 0.0f, S * 0.632f);

    glm::vec3 dx(cs * tallW, 0.0f, sn * tallW);
    glm::vec3 dy(0.0f, tallH, 0.0f);
    glm::vec3 dz(-sn * tallW, 0.0f, cs * tallW);

    glm::vec3 p0 = tallCenter;
    glm::vec3 p1 = tallCenter + dx;
    glm::vec3 p2 = tallCenter + dx + dz;
    glm::vec3 p3 = tallCenter + dz;

    w.addTriQuad(p0, dx, dy, white);
    w.addTriQuad(p1, dz, dy, white);
    w.addTriQuad(p2, -dx, dy, white);
    w.addTriQuad(p3, -dz, dy, white);
    w.addTriQuad(p0 + dy, dx, dz, white);

    float     angle2 = glm::radians(-18.0f);
    float     cs2 = cos(angle2), sn2 = sin(angle2);
    float     shortW = S * 0.297f, shortH = S * 0.297f;
    glm::vec3 shortCenter(S * 0.333f, 0.0f, S * 0.305f);

    glm::vec3 dx2(cs2 * shortW, 0.0f, sn2 * shortW);
    glm::vec3 dy2(0.0f, shortH, 0.0f);
    glm::vec3 dz2(-sn2 * shortW, 0.0f, cs2 * shortW);

    glm::vec3 q0 = shortCenter;
    glm::vec3 q1 = shortCenter + dx2;
    glm::vec3 q2 = shortCenter + dx2 + dz2;
    glm::vec3 q3 = shortCenter + dz2;

    w.addTriQuad(q0, dx2, dy2, white);
    w.addTriQuad(q1, dz2, dy2, white);
    w.addTriQuad(q2, -dx2, dy2, white);
    w.addTriQuad(q3, -dz2, dy2, white);
    w.addTriQuad(q0 + dy2, dx2, dz2, white);

    // Models rotated 180° to face the camera.
    constexpr float PI = 3.14159265f;

    uint32_t bunnyMat = w.addMaterial(Material::Principled(glm::vec3(0.9f, 0.7f, 0.3f), 1.0f, 0.224f));
    w.addMesh(requireOBJ("assets/standford-bunny.obj", 80.0f, glm::vec3(S * 0.5f, shortH - 2.6f, S * 0.35f), PI), bunnyMat);

    uint32_t suzanneMat = w.addMaterial(Material::Glass(1.5f));
    w.addMesh(requireOBJ("assets/suzanne.obj", 4.0f, glm::vec3(S * 0.66f, tallH + 4.0f, S * 0.63f), PI), suzanneMat);

    w.create();
    Log::info("Total triangles: {}", w.triangles.size());
    return scene;
}

Scene Scene::SphereWorld() {
    Scene scene;
    scene.name = "Sphere World";
    Log::info("Building scene: {}", scene.name);

    scene.cameraSettings.aspect_ratio = 16.0f / 9.0f;
    scene.cameraSettings.image_width = 1200;
    scene.cameraSettings.max_bounces = 16;
    scene.cameraSettings.vfov = 20.0f;
    scene.cameraSettings.lookfrom = glm::vec3(13.0f, 2.0f, 3.0f);
    scene.cameraSettings.lookat = glm::vec3(0.0f, 0.0f, 0.0f);

    World& w = scene.world;

    uint32_t        ground = w.addMaterial(Material::Lambertian(glm::vec3(0.5f, 0.5f, 0.5f)));
    constexpr float groundSpan = 50.0f;
    w.addTriQuad(
        glm::vec3(-groundSpan, 0.0f, groundSpan), glm::vec3(2.0f * groundSpan, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -2.0f * groundSpan), ground);
    w.addSphere(glm::vec3(0.0f, 100.0f, 50.0f), 30.0f, Material::Emissive(glm::vec3(1.0f), glm::vec3(10.0f)), 16, 32);

    constexpr int tinyLat = 16;
    constexpr int tinyLon = 32;
    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            float     choose_mat = randomFloat();
            glm::vec3 center = glm::vec3(a + 0.9f * randomFloat(), 0.2f, b + 0.9f * randomFloat());
            if (choose_mat < 0.8f) {
                glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                w.addSphere(center, 0.2f, Material::Lambertian(color), tinyLat, tinyLon);
            } else if (choose_mat < 0.95f) {
                glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                float     roughness = 0.7f * randomFloat();
                w.addSphere(center, 0.2f, Material::Principled(color, 1.0f, roughness), tinyLat, tinyLon);
            } else if (choose_mat < 0.99f) {
                w.addSphere(center, 0.2f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f)), tinyLat, tinyLon);
            } else {
                w.addSphere(center, 0.2f, Material::Glass(1.5f), tinyLat, tinyLon);
            }
        }
    }

    w.addSphere(glm::vec3(0.0f, 1.0f, 4.0f), 1.0f, Material::Glass(1.5f), tinyLat, tinyLon);
    w.addSphere(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f, Material::Principled(glm::vec3(0.7f, 0.6f, 0.5f), 1.0f, 0.0f), tinyLat, tinyLon);
    w.addSphere(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(10.0f, 6.0f, 2.0f)), tinyLat, tinyLon);
    w.addSphere(glm::vec3(-8.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(10.0f, 6.0f, 2.0f)), tinyLat, tinyLon);

    uint32_t  triMat = w.addMaterial(Material::Lambertian(glm::vec3(0.2f, 0.8f, 0.2f)));
    glm::vec3 a(6.0f, 0.0f, -3.0f), b(8.0f, 0.0f, -3.0f), c(7.0f, 0.0f, -5.0f), apex(7.0f, 2.0f, -4.0f);
    w.addTriangle(a, b, apex, triMat);
    w.addTriangle(b, c, apex, triMat);
    w.addTriangle(c, a, apex, triMat);
    w.addTriangle(a, c, b, triMat);

    w.create();
    Log::info("Total triangles: {}", w.triangles.size());
    return scene;
}

Scene Scene::Showcase() {
    Scene scene;
    scene.name = "Showcase";
    Log::info("Building scene: {}", scene.name);

    scene.cameraSettings.aspect_ratio = 16.0f / 9.0f;
    scene.cameraSettings.image_width = 1200;
    scene.cameraSettings.max_bounces = 16;
    scene.cameraSettings.vfov = 30.0f;
    scene.cameraSettings.lookfrom = glm::vec3(0.0f, 3.0f, 10.0f);
    scene.cameraSettings.lookat = glm::vec3(0.0f, 1.0f, 0.0f);

    World& w = scene.world;

    uint32_t        groundMat = w.addMaterial(Material::Lambertian(glm::vec3(0.4f, 0.4f, 0.4f)));
    constexpr float groundSpan = 50.0f;
    w.addTriQuad(
        glm::vec3(-groundSpan, 0.0f, groundSpan), glm::vec3(2.0f * groundSpan, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -2.0f * groundSpan), groundMat);
    w.addSphere(glm::vec3(0.0f, 12.0f, 0.0f), 4.0f, Material::Emissive(glm::vec3(1.0f), glm::vec3(6.0f)));

    // Loaded in object space and placed by transform: a second copy is one more addObject().
    auto place = [&](const char* name, const char* path, uint32_t material, float scale, glm::vec3 offset) {
        const uint32_t meshId = w.addMeshAsset(requireOBJ(path));
        return w.addObject(name, meshId, glm::scale(glm::translate(glm::mat4(1.0f), offset), glm::vec3(scale)), material);
    };

    uint32_t bunnyMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.7f, 0.2f)));
    place("Bunny", "assets/standford-bunny.obj", bunnyMat, 10.0f, glm::vec3(0.0f, -0.33f, 0.0f));

    uint32_t spotMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.85f, 0.7f)));
    place("Spot", "assets/spot.obj", spotMat, 1.0f, glm::vec3(-3.0f, 0.737f, 0.0f));

    uint32_t suzanneMat = w.addMaterial(Material::Principled(glm::vec3(0.9f, 0.7f, 0.3f), 1.0f, 0.316f));
    place("Suzanne", "assets/suzanne.obj", suzanneMat, 0.7f, glm::vec3(4.75f, 0.6f, -2.87f));

    uint32_t dragonMat = w.addMaterial(Material::Glass(1.5f));
    place("Dragon", "assets/xyzrgb_dragon.obj", dragonMat, 0.015f, glm::vec3(0.0f, 0.94f, -2.5f));

    w.addSphere(glm::vec3(2.5f, 0.5f, 2.0f), 0.5f, Material::Glass(1.5f));

    w.create();
    Log::info("Total triangles: {}", w.triangles.size());
    return scene;
}

Scene Scene::MirrorFloor() {
    Scene scene;
    scene.name = "Mirror Floor";
    Log::info("Building scene: {}", scene.name);

    scene.cameraSettings.aspect_ratio = 16.0f / 9.0f;
    scene.cameraSettings.image_width = 1600;
    scene.cameraSettings.max_bounces = 16;
    scene.cameraSettings.vfov = 30.0f;
    scene.cameraSettings.lookfrom = glm::vec3(0.0f, 3.0f, 10.0f);
    scene.cameraSettings.lookat = glm::vec3(0.0f, 1.0f, 0.0f);

    World& w = scene.world;

    constexpr float subjectsZ = 0.0f;
    constexpr float wallZ = -2.5f;
    constexpr float wallSpanX = 8.0f;
    constexpr float wallH = 6.0f;
    constexpr float lightY = 8.0f;
    constexpr float floorY = 0.0f;
    constexpr float floorSpan = 25.0f;
    constexpr float floorFwd = 25.0f;

    // ~4k triangles per sphere; the light shares the density since it's one sphere off the BVH hot path.
    constexpr int sphereLat = 32;
    constexpr int sphereLon = 64;

    // Rests each mesh on the floor by its lowest vertex, since authoring origins differ per mesh.
    auto loadStanding = [&](const std::filesystem::path& path, float scale, float x, float z, float rotateY = 0.0f) -> Mesh {
        Mesh  m = requireOBJ(path, scale, glm::vec3(x, 0.0f, z), rotateY);
        float yMin = std::numeric_limits<float>::infinity();
        for (const auto& v : m.vertices) {
            yMin = std::min(yMin, v.position.y);
        }
        const float lift = floorY - yMin;
        for (auto& v : m.vertices) {
            v.position.y += lift;
        }
        return m;
    };

    // v points toward the wall (-z) so cross(u, v) faces +y.
    uint32_t floorMat = w.addMaterial(Material::Principled(glm::vec3(0.95f, 0.95f, 0.95f), 1.0f, 0.0f));
    w.addTriQuad(glm::vec3(-floorSpan, floorY, floorFwd), glm::vec3(2.0f * floorSpan, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, wallZ - floorFwd), floorMat);

    uint32_t wallMat = w.addMaterial(Material::Lambertian(glm::vec3(0.55f, 0.6f, 0.65f)));
    w.addTriQuad(glm::vec3(-wallSpanX, floorY, wallZ), glm::vec3(2.0f * wallSpanX, 0.0f, 0.0f), glm::vec3(0.0f, wallH, 0.0f), wallMat);

    // Large radius gives ReSTIR/NEE a soft target; small lights mean harder shadows and more variance.
    w.addSphere(glm::vec3(0.0f, lightY, subjectsZ), 3.0f, Material::Emissive(glm::vec3(1.0f), glm::vec3(6.0f)), sphereLat, sphereLon);

    uint32_t suzanneMat = w.addMaterial(Material::Lambertian(glm::vec3(0.85f, 0.35f, 0.25f)));
    w.addMesh(loadStanding("assets/suzanne.obj", 1.0f, -0.5f, -5.0f), suzanneMat);

    uint32_t dragonMat = w.addMaterial(Material::Principled(glm::vec3(0.9f, 0.75f, 0.4f), 1.0f, 0.224f));
    w.addMesh(loadStanding("assets/xyzrgb_dragon.obj", 0.02f, 0.0f, subjectsZ), dragonMat);

    w.addSphere(glm::vec3(3.5f, floorY + 1.0f, subjectsZ), 1.0f, Material::Lambertian(glm::vec3(0.25f, 0.55f, 0.8f)), sphereLat, sphereLon);

    w.create();
    Log::info("Total triangles: {}", w.triangles.size());
    return scene;
}

constexpr const char* DEFAULT_ENV_MAP = "assets/env/kloofendal_overcast_puresky_1k.hdr";

Scene Scene::SphereWorldEnvLit() {
    Scene scene;
    scene.name = "Sphere World (env)";
    Log::info("Building scene: {}", scene.name);

    scene.cameraSettings.aspect_ratio = 16.0f / 9.0f;
    scene.cameraSettings.image_width = 1200;
    scene.cameraSettings.max_bounces = 16;
    scene.cameraSettings.vfov = 20.0f;
    scene.cameraSettings.lookfrom = glm::vec3(13.0f, 2.0f, 3.0f);
    scene.cameraSettings.lookat = glm::vec3(0.0f, 0.0f, 0.0f);

    scene.envMapPath = DEFAULT_ENV_MAP;
    scene.envIntensity = 1.0f;

    World& w = scene.world;

    uint32_t        ground = w.addMaterial(Material::Lambertian(glm::vec3(0.5f, 0.5f, 0.5f)));
    constexpr float groundSpan = 50.0f;
    w.addTriQuad(
        glm::vec3(-groundSpan, 0.0f, groundSpan), glm::vec3(2.0f * groundSpan, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -2.0f * groundSpan), ground);

    constexpr int tinyLat = 16;
    constexpr int tinyLon = 32;
    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            float     choose_mat = randomFloat();
            glm::vec3 center = glm::vec3(a + 0.9f * randomFloat(), 0.2f, b + 0.9f * randomFloat());
            if (choose_mat < 0.8f) {
                glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                w.addSphere(center, 0.2f, Material::Lambertian(color), tinyLat, tinyLon);
            } else if (choose_mat < 0.95f) {
                glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                float     roughness = 0.7f * randomFloat();
                w.addSphere(center, 0.2f, Material::Principled(color, 1.0f, roughness), tinyLat, tinyLon);
            } else {
                w.addSphere(center, 0.2f, Material::Glass(1.5f), tinyLat, tinyLon);
            }
        }
    }

    w.addSphere(glm::vec3(0.0f, 1.0f, 4.0f), 1.0f, Material::Glass(1.5f), tinyLat, tinyLon);
    w.addSphere(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f, Material::Principled(glm::vec3(0.7f, 0.6f, 0.5f), 1.0f, 0.0f), tinyLat, tinyLon);
    w.addSphere(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, Material::Lambertian(glm::vec3(0.4f, 0.2f, 0.1f)), tinyLat, tinyLon);

    w.create();
    Log::info("Total triangles: {}", w.triangles.size());
    return scene;
}

Scene Scene::ShowcaseEnvLit() {
    Scene scene;
    scene.name = "Showcase (env)";
    Log::info("Building scene: {}", scene.name);

    scene.cameraSettings.aspect_ratio = 16.0f / 9.0f;
    scene.cameraSettings.image_width = 1200;
    scene.cameraSettings.max_bounces = 16;
    scene.cameraSettings.vfov = 30.0f;
    scene.cameraSettings.lookfrom = glm::vec3(0.0f, 3.0f, 10.0f);
    scene.cameraSettings.lookat = glm::vec3(0.0f, 1.0f, 0.0f);

    scene.envMapPath = DEFAULT_ENV_MAP;
    scene.envIntensity = 1.0f;

    World& w = scene.world;

    uint32_t        groundMat = w.addMaterial(Material::Lambertian(glm::vec3(0.4f, 0.4f, 0.4f)));
    constexpr float groundSpan = 50.0f;
    w.addTriQuad(
        glm::vec3(-groundSpan, 0.0f, groundSpan), glm::vec3(2.0f * groundSpan, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -2.0f * groundSpan), groundMat);

    uint32_t bunnyMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.7f, 0.2f)));
    w.addMesh(requireOBJ("assets/standford-bunny.obj", 10.0f, glm::vec3(0.0f, -0.33f, 0.0f)), bunnyMat);

    uint32_t spotMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.85f, 0.7f)));
    w.addMesh(requireOBJ("assets/spot.obj", 1.0f, glm::vec3(-3.0f, 0.737f, 0.0f)), spotMat);

    uint32_t suzanneMat = w.addMaterial(Material::Principled(glm::vec3(0.9f, 0.7f, 0.3f), 1.0f, 0.316f));
    w.addMesh(requireOBJ("assets/suzanne.obj", 0.7f, glm::vec3(4.75f, 0.6f, -2.87f)), suzanneMat);

    uint32_t dragonMat = w.addMaterial(Material::Glass(1.5f));
    w.addMesh(requireOBJ("assets/xyzrgb_dragon.obj", 0.015f, glm::vec3(0.0f, 0.94f, -2.5f)), dragonMat);

    w.addSphere(glm::vec3(2.5f, 0.5f, 2.0f), 0.5f, Material::Glass(1.5f));

    w.create();
    Log::info("Total triangles: {}", w.triangles.size());
    return scene;
}

Scene Scene::MaterialGallery() {
    Scene scene;
    scene.name = "Material Gallery";
    Log::info("Building scene: {}", scene.name);

    scene.cameraSettings.aspect_ratio = 16.0f / 9.0f;
    scene.cameraSettings.image_width = 1200;
    scene.cameraSettings.max_bounces = 24;
    scene.cameraSettings.vfov = 34.0f;
    scene.cameraSettings.lookfrom = glm::vec3(0.0f, 4.6f, 11.5f);
    scene.cameraSettings.lookat = glm::vec3(0.0f, 0.5f, 0.0f);

    // Envmap plus emitters on purpose: the env gives soft reflections for the roughness sweep, and the
    // emitters give crisp highlights and are the only thing that exercises NEE and ReSTIR.
    scene.envMapPath = DEFAULT_ENV_MAP;
    scene.envIntensity = 0.45f;

    World& w = scene.world;

    constexpr int   latSegs = 16;
    constexpr int   lonSegs = 32;
    constexpr int   columns = 7;
    constexpr float radius = 0.5f;
    constexpr float spacingX = 1.35f;
    constexpr float spacingZ = 1.6f;

    // Glossy dielectric ground so every row sits in a reflection that reveals its silhouette.
    constexpr float groundSpan = 40.0f;
    w.addTriQuad(glm::vec3(-groundSpan, 0.0f, groundSpan),
                 glm::vec3(2.0f * groundSpan, 0.0f, 0.0f),
                 glm::vec3(0.0f, 0.0f, -2.0f * groundSpan),
                 Material::Principled(glm::vec3(0.32f, 0.32f, 0.34f), 0.0f, 0.4f));

    auto sweepRow = [&](float z, auto&& materialAt) {
        for (int i = 0; i < columns; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(columns - 1);
            const float x = (static_cast<float>(i) - 0.5f * static_cast<float>(columns - 1)) * spacingX;
            w.addSphere(glm::vec3(x, radius, z), radius, materialAt(t), latSegs, lonSegs);
        }
    };

    sweepRow(-2.0f * spacingZ, [](float t) { return Material::Principled(glm::vec3(0.95f, 0.78f, 0.35f), 1.0f, t); });

    // Base colour stays put while the highlight spreads: a dielectric's F0 comes from ior, not albedo.
    sweepRow(-1.0f * spacingZ, [](float t) { return Material::Principled(glm::vec3(0.16f, 0.34f, 0.78f), 0.0f, t); });

    // Neutral base colour so the transition reads as diffuse giving way to tinted specular, not a colour change.
    sweepRow(0.0f, [](float t) { return Material::Principled(glm::vec3(0.85f, 0.85f, 0.88f), t, 0.25f); });

    sweepRow(1.0f * spacingZ, [](float t) { return Material::RoughGlass(1.5f, 0.45f * t); });

    sweepRow(2.0f * spacingZ, [](float t) { return Material::Glass(1.05f + t * 1.35f); });

    w.addSphere(glm::vec3(-3.7f, 0.95f, 4.3f), 0.95f, Material::RoughGlass(1.5f, 0.05f, glm::vec3(0.95f, 0.55f, 0.20f)), 24, 48);
    w.addSphere(glm::vec3(3.7f, 0.95f, 4.3f), 0.95f, Material::RoughGlass(1.5f, 0.22f, glm::vec3(0.25f, 0.85f, 0.45f)), 24, 48);

    // Two lights so conductors carry two distinguishable highlights whose smear shows roughness.
    w.addSphere(glm::vec3(-6.5f, 7.5f, 4.0f), 1.2f, Material::Emissive(glm::vec3(1.0f), glm::vec3(11.0f, 10.0f, 8.5f)), 16, 32);
    w.addSphere(glm::vec3(7.0f, 4.5f, 6.0f), 0.8f, Material::Emissive(glm::vec3(1.0f), glm::vec3(3.0f, 4.2f, 6.5f)), 16, 32);

    w.create();
    Log::info("Total triangles: {}", w.triangles.size());
    return scene;
}

std::vector<SceneEntry> sceneRegistry() {
    return {
        {"Cornell Box", &Scene::CornellBox},
        {"Sphere World", &Scene::SphereWorld},
        {"Sphere World (env)", &Scene::SphereWorldEnvLit},
        {"Showcase", &Scene::Showcase},
        {"Showcase (env)", &Scene::ShowcaseEnvLit},
        {"Mirror Floor", &Scene::MirrorFloor},
        {"Material Gallery", &Scene::MaterialGallery},
    };
}
