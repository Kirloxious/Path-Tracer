#include "scene.h"

#include "log.h"
#include "obj_loader.h"
#include "utils.h"

Scene Scene::CornellBox() {
    Scene scene;
    scene.name = "Cornell Box";
    Log::info("Building scene: {}", scene.name);

    scene.cameraSettings.aspect_ratio = 16.0f / 9.0f;
    scene.cameraSettings.image_width = 1200;
    scene.cameraSettings.max_bounces = 16;
    scene.cameraSettings.samples_per_pixel = 4;
    scene.cameraSettings.vfov = 40.0f;
    scene.cameraSettings.lookfrom = glm::vec3(27.75f, 27.75f, -75.0f);
    scene.cameraSettings.lookat = glm::vec3(27.75f, 27.75f, 27.75f);

    World& w = scene.world;

    constexpr float S = 55.5f;

    uint32_t white = w.addMaterial(Material::Lambertian(glm::vec3(0.73f, 0.73f, 0.73f)));
    uint32_t red = w.addMaterial(Material::Lambertian(glm::vec3(0.65f, 0.05f, 0.05f)));
    uint32_t green = w.addMaterial(Material::Lambertian(glm::vec3(0.12f, 0.45f, 0.15f)));

    // Ceiling light
    w.addSphere(glm::vec3(S * 0.5f, S * 0.93f, S * 0.5f), S * 0.06f, Material::Emissive(glm::vec3(1.0f), glm::vec3(15.0f)));

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

    uint32_t bunnyMat = w.addMaterial(Material::Metal(glm::vec3(0.9f, 0.7f, 0.3f), 0.05f));
    w.addMesh(loadOBJ("assets/standford-bunny.obj", bunnyMat, 80.0f, glm::vec3(S * 0.5f, shortH - 2.6f, S * 0.35f), PI));

    uint32_t suzanneMat = w.addMaterial(Material::Dielectric(1.5f));
    w.addMesh(loadOBJ("assets/suzanne.obj", suzanneMat, 4.0f, glm::vec3(S * 0.66f, tallH + 4.0f, S * 0.63f), PI));

    w.emissiveLastIndex = w.sortEmissiveFirst();
    Log::info("Emissive last index: {}", w.emissiveLastIndex);
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
    scene.cameraSettings.samples_per_pixel = 4;
    scene.cameraSettings.vfov = 20.0f;
    scene.cameraSettings.lookfrom = glm::vec3(13.0f, 2.0f, 3.0f);
    scene.cameraSettings.lookat = glm::vec3(0.0f, 0.0f, 0.0f);

    World& w = scene.world;

    uint32_t ground = w.addMaterial(Material::Lambertian(glm::vec3(0.5f, 0.5f, 0.5f)));
    w.addSphere(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, ground);
    w.addSphere(glm::vec3(0.0f, 100.0f, 50.0f), 30.0f, Material::Emissive(glm::vec3(1.0f), glm::vec3(4.0f)));

    // Tiny radius-0.2 spheres are mostly sub-pixel — drop tessellation to 4x6
    // (16 tris each) instead of the 224-tri default to keep the BVH manageable.
    constexpr int tinyLat = 4;
    constexpr int tinyLon = 6;
    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            float     choose_mat = randomFloat();
            glm::vec3 center = glm::vec3(a + 0.9f * randomFloat(), 0.2f, b + 0.9f * randomFloat());
            if (choose_mat < 0.8f) {
                glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                w.addSphere(center, 0.2f, Material::Lambertian(color), tinyLat, tinyLon);
            } else if (choose_mat < 0.95f) {
                glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                float     fuzz = 0.5f * randomFloat();
                w.addSphere(center, 0.2f, Material::Metal(color, fuzz), tinyLat, tinyLon);
            } else if (choose_mat < 0.99f) {
                w.addSphere(center, 0.2f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f)), tinyLat, tinyLon);
            } else {
                w.addSphere(center, 0.2f, Material::Dielectric(1.5f), tinyLat, tinyLon);
            }
        }
    }

    w.addSphere(glm::vec3(0.0f, 1.0f, 4.0f), 1.0f, Material::Dielectric(1.5f));
    w.addSphere(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f, Material::Metal(glm::vec3(0.7f, 0.6f, 0.5f), 0.0f));
    w.addSphere(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(10.0f, 6.0f, 2.0f)));
    w.addSphere(glm::vec3(-8.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(10.0f, 6.0f, 2.0f)));

    uint32_t  triMat = w.addMaterial(Material::Lambertian(glm::vec3(0.2f, 0.8f, 0.2f)));
    glm::vec3 a(6.0f, 0.0f, -3.0f), b(8.0f, 0.0f, -3.0f), c(7.0f, 0.0f, -5.0f), apex(7.0f, 2.0f, -4.0f);
    w.addTriangle(a, b, apex, triMat);
    w.addTriangle(b, c, apex, triMat);
    w.addTriangle(c, a, apex, triMat);
    w.addTriangle(a, c, b, triMat);

    w.emissiveLastIndex = w.sortEmissiveFirst();
    Log::info("Emissive last index: {}", w.emissiveLastIndex);

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
    scene.cameraSettings.samples_per_pixel = 4;
    scene.cameraSettings.vfov = 30.0f;
    scene.cameraSettings.lookfrom = glm::vec3(0.0f, 3.0f, 10.0f);
    scene.cameraSettings.lookat = glm::vec3(0.0f, 1.0f, 0.0f);

    World& w = scene.world;

    w.addSphere(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, Material::Lambertian(glm::vec3(0.4f, 0.4f, 0.4f)));
    w.addSphere(glm::vec3(0.0f, 12.0f, 0.0f), 4.0f, Material::Emissive(glm::vec3(1.0f), glm::vec3(6.0f)));

    uint32_t bunnyMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.7f, 0.2f)));
    w.addMesh(loadOBJ("assets/standford-bunny.obj", bunnyMat, 10.0f, glm::vec3(0.0f, -0.33f, 0.0f)));

    uint32_t spotMat = w.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.85f, 0.7f)));
    w.addMesh(loadOBJ("assets/spot.obj", spotMat, 1.0f, glm::vec3(-3.0f, 0.737f, 0.0f)));

    uint32_t suzanneMat = w.addMaterial(Material::Metal(glm::vec3(0.9f, 0.7f, 0.3f), 0.1f));
    w.addMesh(loadOBJ("assets/suzanne.obj", suzanneMat, 0.7f, glm::vec3(4.75f, 0.6f, -2.87f)));

    uint32_t dragonMat = w.addMaterial(Material::Dielectric(1.5f));
    w.addMesh(loadOBJ("assets/xyzrgb_dragon.obj", dragonMat, 0.015f, glm::vec3(0.0f, 0.94f, -2.5f)));

    w.addSphere(glm::vec3(2.5f, 0.5f, 2.0f), 0.5f, Material::Dielectric(1.5f));

    w.emissiveLastIndex = w.sortEmissiveFirst();
    Log::info("Emissive last index: {}", w.emissiveLastIndex);
    Log::info("Total triangles: {}", w.triangles.size());

    w.create();
    return scene;
}

std::vector<SceneEntry> sceneRegistry() {
    return {
        {"Cornell Box", &Scene::CornellBox},
        {"Sphere World", &Scene::SphereWorld},
        {"Showcase", &Scene::Showcase},
    };
}
