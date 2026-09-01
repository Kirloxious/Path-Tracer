#include "scene/scene.h"

#include <algorithm>
#include <limits>

#include "core/log.h"
#include "scene/obj_loader.h"
#include "core/utils.h"

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

    // Ceiling light
    w.addSphere(glm::vec3(S * 0.5f, S * 0.93f, S * 0.5f), S * 0.06f, Material::Emissive(glm::vec3(1.0f), glm::vec3(8.0f)), 16, 32);

    // Walls — open front at z=0
    w.addTriQuad(glm::vec3(S, 0, 0), glm::vec3(0, 0, S), glm::vec3(0, S, 0), red);   // Left (red)
    w.addTriQuad(glm::vec3(0, 0, 0), glm::vec3(0, S, 0), glm::vec3(0, 0, S), green); // Right (green)
    w.addTriQuad(glm::vec3(0, 0, 0), glm::vec3(0, 0, S), glm::vec3(S, 0, 0), white); // Floor
    w.addTriQuad(glm::vec3(0, S, 0), glm::vec3(S, 0, 0), glm::vec3(0, 0, S), white); // Ceiling
    w.addTriQuad(glm::vec3(0, 0, S), glm::vec3(0, S, 0), glm::vec3(S, 0, 0), white); // Back wall

    // Tall box — rotated ~15 degrees
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

    // Short box — rotated ~-18 degrees
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

    // Small models — rotated 180 to face the camera
    constexpr float PI = 3.14159265f;

    uint32_t bunnyMat = w.addMaterial(Material::Principled(glm::vec3(0.9f, 0.7f, 0.3f), 1.0f, 0.224f));
    w.addMesh(loadOBJ("assets/standford-bunny.obj", bunnyMat, 80.0f, glm::vec3(S * 0.5f, shortH - 2.6f, S * 0.35f), PI));

    uint32_t suzanneMat = w.addMaterial(Material::Glass(1.5f));
    w.addMesh(loadOBJ("assets/suzanne.obj", suzanneMat, 4.0f, glm::vec3(S * 0.66f, tallH + 4.0f, S * 0.63f), PI));

    w.sortEmissiveFirst();
    Log::info("Total triangles: {}", w.triangles.size());

    w.create();
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

    w.sortEmissiveFirst();

    w.create();
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

    uint32_t bunnyMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.7f, 0.2f)));
    w.addMesh(loadOBJ("assets/standford-bunny.obj", bunnyMat, 10.0f, glm::vec3(0.0f, -0.33f, 0.0f)));

    uint32_t spotMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.85f, 0.7f)));
    w.addMesh(loadOBJ("assets/spot.obj", spotMat, 1.0f, glm::vec3(-3.0f, 0.737f, 0.0f)));

    uint32_t suzanneMat = w.addMaterial(Material::Principled(glm::vec3(0.9f, 0.7f, 0.3f), 1.0f, 0.316f));
    w.addMesh(loadOBJ("assets/suzanne.obj", suzanneMat, 0.7f, glm::vec3(4.75f, 0.6f, -2.87f)));

    uint32_t dragonMat = w.addMaterial(Material::Glass(1.5f));
    w.addMesh(loadOBJ("assets/xyzrgb_dragon.obj", dragonMat, 0.015f, glm::vec3(0.0f, 0.94f, -2.5f)));

    w.addSphere(glm::vec3(2.5f, 0.5f, 2.0f), 0.5f, Material::Glass(1.5f));

    w.sortEmissiveFirst();
    Log::info("Total triangles: {}", w.triangles.size());

    w.create();
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

    // Layout: three subjects in a line at z=0, backsplash wall parallel behind at z=-2.5,
    // area light centred on x=0 directly above. Camera looks down the +z axis so the wall
    // frames all three subjects.
    constexpr float subjectsZ = 0.0f;
    constexpr float wallZ = -2.5f;
    constexpr float wallSpanX = 8.0f; // wall extends from -wallSpanX to +wallSpanX
    constexpr float wallH = 6.0f;
    constexpr float lightY = 8.0f;
    constexpr float floorY = 0.0f;
    constexpr float floorSpan = 25.0f; // floor extends from -floorSpan to +floorSpan in x
    constexpr float floorFwd = 25.0f;  // ... and from z = wallZ at the back to z = floorFwd in front of the camera

    // Sphere tessellation. Triangles per sphere = 2 * lat * lon (minus the degenerate
    // pole row). 32 × 64 = ~4k tris per sphere — silhouette is smooth at the radius-1
    // subject sphere's screen size; bump higher if you push the camera in closer.
    // The light sphere uses the same density: visible mostly as a soft disc, but the
    // silhouette still benefits from extra subdivision and the cost is irrelevant
    // (one sphere, no BVH hot path).
    constexpr int sphereLat = 32;
    constexpr int sphereLon = 64;

    // Load an OBJ at (x, z) and shift it vertically so its lowest vertex sits exactly at floorY —
    // each mesh's authoring origin differs (Suzanne is centred, the dragon's pivot is its underside),
    // so hand-tuning y per-mesh is fragile. This stays correct under any scale or Y-rotation.
    auto loadStanding = [&](const std::filesystem::path& path, uint32_t mat, float scale, float x, float z, float rotateY = 0.0f) -> OBJMesh {
        OBJMesh m = loadOBJ(path, mat, scale, glm::vec3(x, 0.0f, z), rotateY);
        float   yMin = std::numeric_limits<float>::infinity();
        for (const auto& v : m.vertices) {
            yMin = std::min(yMin, v.position.y);
        }
        const float lift = floorY - yMin;
        for (auto& v : m.vertices) {
            v.position.y += lift;
        }
        return m;
    };

    // Mirror floor: a single flat quad. addTriQuad winds CCW around cross(u, v), so picking
    // u = +x and v pointing back toward the wall (negative z) makes the front face point at +y.
    // fuzz=0 keeps reflections sharp; near-white albedo preserves the reflected colours.
    uint32_t floorMat = w.addMaterial(Material::Principled(glm::vec3(0.95f, 0.95f, 0.95f), 1.0f, 0.0f));
    w.addTriQuad(glm::vec3(-floorSpan, floorY, floorFwd), glm::vec3(2.0f * floorSpan, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, wallZ - floorFwd), floorMat);

    // Backsplash wall: a single quad behind the subjects, normal facing the camera (+z).
    // u=+x, v=+y so cross(u, v) = +z. Cool-grey lambertian gives a neutral frame against the warm subjects.
    uint32_t wallMat = w.addMaterial(Material::Lambertian(glm::vec3(0.55f, 0.6f, 0.65f)));
    w.addTriQuad(glm::vec3(-wallSpanX, floorY, wallZ), glm::vec3(2.0f * wallSpanX, 0.0f, 0.0f), glm::vec3(0.0f, wallH, 0.0f), wallMat);

    // Overhead area light, centred above the row of subjects. Radius 3 (large enough that
    // ReSTIR/NEE have a soft target — small lights give harder shadows + more variance);
    // emission 6 matches the previous brightness given the closer placement.
    w.addSphere(glm::vec3(0.0f, lightY, subjectsZ), 3.0f, Material::Emissive(glm::vec3(1.0f), glm::vec3(6.0f)), sphereLat, sphereLon);

    // Lambertian Suzanne on the left — lifted so her chin sits on the mirror.
    uint32_t suzanneMat = w.addMaterial(Material::Lambertian(glm::vec3(0.85f, 0.35f, 0.25f)));
    w.addMesh(loadStanding("assets/suzanne.obj", suzanneMat, 1.0f, -0.5f, -5.0f));

    // Metal gold dragon in the middle — fuzz softens the highlights without going full mirror.
    uint32_t dragonMat = w.addMaterial(Material::Principled(glm::vec3(0.9f, 0.75f, 0.4f), 1.0f, 0.224f));
    w.addMesh(loadStanding("assets/xyzrgb_dragon.obj", dragonMat, 0.02f, 0.0f, subjectsZ));

    // Lambertian sphere on the right — radius 1, centre at y = floorY + 1, so its bottom sits exactly on the floor.
    w.addSphere(glm::vec3(3.5f, floorY + 1.0f, subjectsZ), 1.0f, Material::Lambertian(glm::vec3(0.25f, 0.55f, 0.8f)), sphereLat, sphereLon);

    w.sortEmissiveFirst();
    Log::info("Total triangles: {}", w.triangles.size());

    w.create();
    return scene;
}

static const char* kDefaultEnvMap = "assets/env/kloofendal_overcast_puresky_1k.hdr";

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

    scene.envMapPath = kDefaultEnvMap;
    scene.envIntensity = 1.0f;

    World& w = scene.world;

    uint32_t        ground = w.addMaterial(Material::Lambertian(glm::vec3(0.5f, 0.5f, 0.5f)));
    constexpr float groundSpan = 50.0f;
    w.addTriQuad(
        glm::vec3(-groundSpan, 0.0f, groundSpan), glm::vec3(2.0f * groundSpan, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -2.0f * groundSpan), ground);
    // No overhead sun sphere and no red emissives — env lights everything.

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

    w.sortEmissiveFirst();

    w.create();
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

    scene.envMapPath = kDefaultEnvMap;
    scene.envIntensity = 1.0f;

    World& w = scene.world;

    uint32_t        groundMat = w.addMaterial(Material::Lambertian(glm::vec3(0.4f, 0.4f, 0.4f)));
    constexpr float groundSpan = 50.0f;
    w.addTriQuad(
        glm::vec3(-groundSpan, 0.0f, groundSpan), glm::vec3(2.0f * groundSpan, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -2.0f * groundSpan), groundMat);
    // Overhead emissive sun removed — env carries the lighting.

    uint32_t bunnyMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.7f, 0.2f)));
    w.addMesh(loadOBJ("assets/standford-bunny.obj", bunnyMat, 10.0f, glm::vec3(0.0f, -0.33f, 0.0f)));

    uint32_t spotMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.85f, 0.7f)));
    w.addMesh(loadOBJ("assets/spot.obj", spotMat, 1.0f, glm::vec3(-3.0f, 0.737f, 0.0f)));

    uint32_t suzanneMat = w.addMaterial(Material::Principled(glm::vec3(0.9f, 0.7f, 0.3f), 1.0f, 0.316f));
    w.addMesh(loadOBJ("assets/suzanne.obj", suzanneMat, 0.7f, glm::vec3(4.75f, 0.6f, -2.87f)));

    uint32_t dragonMat = w.addMaterial(Material::Glass(1.5f));
    w.addMesh(loadOBJ("assets/xyzrgb_dragon.obj", dragonMat, 0.015f, glm::vec3(0.0f, 0.94f, -2.5f)));

    w.addSphere(glm::vec3(2.5f, 0.5f, 2.0f), 0.5f, Material::Glass(1.5f));

    w.sortEmissiveFirst();
    Log::info("Total triangles: {}", w.triangles.size());

    w.create();
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
    };
}
