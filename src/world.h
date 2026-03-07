#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>

#include "log.h"

#include "bvh.h"
#include "utils.h"
#include "material.h"
#include "obj_loader.h"

class World
{
private:
    void create() { bvh.build(spheres, triangles); }

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

    int sortEmissiveFirst() {
        auto it =
            std::stable_partition(spheres.begin(), spheres.end(), [&](const Sphere& s) { return (int)materials[s.material_index].isEmissive(); });
        return (int)std::distance(spheres.begin(), it) - 1; // last emissive index
    }

    static World buildSphereWorld() {
        World world;

        // Ground sphere and mat
        uint32_t ground = world.addMaterial(Material::Lambertian(glm::vec3(0.5f, 0.5f, 0.5f)));
        world.addSphere(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, ground);

        world.addSphere(glm::vec3(0.0f, 100.0f, 50.0f), 30.0f, Material::Emissive(glm::vec3(1.0f), glm::vec3(4.0f)));

        for (int a = -11; a < 11; a++) {
            for (int b = -11; b < 11; b++) {
                float     choose_mat = randomFloat();
                glm::vec3 center = glm::vec3(a + 0.9f * randomFloat(), 0.2f, b + 0.9f * randomFloat());
                if (choose_mat < 0.8f) {
                    // diffuse
                    glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                    world.addSphere(center, 0.2f, Material::Lambertian(color));
                } else if (choose_mat < 0.95f) {
                    // metal
                    glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                    float     fuzz = 0.5f * randomFloat();
                    world.addSphere(center, 0.2f, Material::Metal(color, fuzz));
                } else if (choose_mat < 0.99f) {
                    world.addSphere(center, 0.2f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f)));
                } else {
                    // glass
                    world.addSphere(center, 0.2f, Material::Dielectric(1.5f));
                }
            }
        }

        // Big spheres

        // Glasss
        world.addSphere(glm::vec3(0.0f, 1.0f, 4.0f), 1.0f, Material::Dielectric(1.5f));
        // Mirror
        world.addSphere(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f, Material::Metal(glm::vec3(0.7f, 0.6f, 0.5f), 0.0f));
        // Brown
        //  world.addSphere(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, Material::Lambertian(glm::vec3(0.4f,
        //  0.2f, 0.1f)));
        world.addSphere(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(10.0f, 6.0f, 2.0f)));

        // Red light
        world.addSphere(glm::vec3(-8.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(10.0f, 6.0f, 2.0f)));

        // Test triangles — a small pyramid
        uint32_t  triMat = world.addMaterial(Material::Lambertian(glm::vec3(0.2f, 0.8f, 0.2f)));
        glm::vec3 a(6.0f, 0.0f, -3.0f), b(8.0f, 0.0f, -3.0f), c(7.0f, 0.0f, -5.0f), apex(7.0f, 2.0f, -4.0f);
        world.addTriangle(a, b, apex, triMat); // front face
        world.addTriangle(b, c, apex, triMat); // right face
        world.addTriangle(c, a, apex, triMat); // left face
        world.addTriangle(a, c, b, triMat);    // base

        world.emissiveLastIndex = world.sortEmissiveFirst();

        Log::info("Emissive last index: {}", world.emissiveLastIndex);

        world.create();

        return world;
    }

    static World buildShowcaseWorld() {
        World world;

        // Ground plane
        world.addSphere(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, Material::Lambertian(glm::vec3(0.4f, 0.4f, 0.4f)));

        // Overhead light
        world.addSphere(glm::vec3(0.0f, 12.0f, 0.0f), 4.0f, Material::Emissive(glm::vec3(1.0f), glm::vec3(6.0f)));

        // Stanford bunny (center) — raw ~0.15 units tall, Y starts at 0.033
        uint32_t bunnyMat = world.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.7f, 0.2f)));
        // uint32_t bunnyMat = world.addMaterial(Material::Emissive(glm::vec3(0.9f, 0.7f, 0.2f), glm::vec3(4.0f, 3.0f, 1.0f)));

        world.addMesh(loadOBJ("assets/standford-bunny.obj", bunnyMat, 10.0f, glm::vec3(0.0f, -0.33f, 0.0f)));

        // Spot the cow (left) — raw ~1.7 units, Y starts at -0.737
        uint32_t spotMat = world.addMaterial(Material::Lambertian(glm::vec3(0.9f, 0.85f, 0.7f)));
        world.addMesh(loadOBJ("assets/spot.obj", spotMat, 1.0f, glm::vec3(-3.0f, 0.737f, 0.0f)));

        // Suzanne (right) — raw ~2.7 units, centered at roughly (-2.5, 1.25, 4.1)
        uint32_t suzanneMat = world.addMaterial(Material::Metal(glm::vec3(0.9f, 0.7f, 0.3f), 0.1f));
        world.addMesh(loadOBJ("assets/suzanne.obj", suzanneMat, 0.7f, glm::vec3(4.75f, 0.6f, -2.87f)));

        // Dragon (back center) — raw ~200 units, center near (2, -7, 10)
        uint32_t dragonMat = world.addMaterial(Material::Dielectric(1.5f));
        world.addMesh(loadOBJ("assets/xyzrgb_dragon.obj", dragonMat, 0.015f, glm::vec3(0.0f, 0.94f, -2.5f)));

        // Glass sphere
        world.addSphere(glm::vec3(2.5f, 0.5f, 2.0f), 0.5f, Material::Dielectric(1.5f));

        world.emissiveLastIndex = world.sortEmissiveFirst();

        Log::info("Emissive last index: {}", world.emissiveLastIndex);
        Log::info("Total triangles: {}", world.triangles.size());

        world.create();

        return world;
    }
    static World buildCornellBox() {
        World world;

        constexpr float S = 55.5f;

        // Materials
        uint32_t white = world.addMaterial(Material::Lambertian(glm::vec3(0.73f, 0.73f, 0.73f)));
        uint32_t red = world.addMaterial(Material::Lambertian(glm::vec3(0.65f, 0.05f, 0.05f)));
        uint32_t green = world.addMaterial(Material::Lambertian(glm::vec3(0.12f, 0.45f, 0.15f)));

        // Ceiling light
        world.addSphere(glm::vec3(S * 0.5f, S * 0.93f, S * 0.5f), S * 0.06f, Material::Emissive(glm::vec3(1.0f), glm::vec3(15.0f)));

        // Walls — open front at z=0
        // Left wall (red)   x = S plane
        world.addTriQuad(glm::vec3(S, 0, 0), glm::vec3(0, 0, S), glm::vec3(0, S, 0), red);
        // Right wall (green) x = 0 plane
        world.addTriQuad(glm::vec3(0, 0, 0), glm::vec3(0, S, 0), glm::vec3(0, 0, S), green);
        // Floor (white)     y = 0 plane
        world.addTriQuad(glm::vec3(0, 0, 0), glm::vec3(0, 0, S), glm::vec3(S, 0, 0), white);
        // Ceiling (white)   y = S plane
        world.addTriQuad(glm::vec3(0, S, 0), glm::vec3(S, 0, 0), glm::vec3(0, 0, S), white);
        // Back wall (white) z = S plane
        world.addTriQuad(glm::vec3(0, 0, S), glm::vec3(0, S, 0), glm::vec3(S, 0, 0), white);

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

        world.addTriQuad(p0, dx, dy, white);
        world.addTriQuad(p1, dz, dy, white);
        world.addTriQuad(p2, -dx, dy, white);
        world.addTriQuad(p3, -dz, dy, white);
        world.addTriQuad(p0 + dy, dx, dz, white);

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

        world.addTriQuad(q0, dx2, dy2, white);
        world.addTriQuad(q1, dz2, dy2, white);
        world.addTriQuad(q2, -dx2, dy2, white);
        world.addTriQuad(q3, -dz2, dy2, white);
        world.addTriQuad(q0 + dy2, dx2, dz2, white);

        // Small floating models — rotated 180° to face the camera
        constexpr float PI = 3.14159265f;

        // Bunny on top of the short box
        uint32_t bunnyMat = world.addMaterial(Material::Metal(glm::vec3(0.9f, 0.7f, 0.3f), 0.05f));
        world.addMesh(loadOBJ("assets/standford-bunny.obj", bunnyMat, 80.0f, glm::vec3(S * 0.5f, shortH - 2.6f, S * 0.35f), PI));

        // Suzanne floating above the tall box
        uint32_t suzanneMat = world.addMaterial(Material::Dielectric(1.5f));
        world.addMesh(loadOBJ("assets/suzanne.obj", suzanneMat, 4.0f, glm::vec3(S * 0.66f, tallH + 4.0f, S * 0.63f), PI));

        world.emissiveLastIndex = world.sortEmissiveFirst();
        Log::info("Emissive last index: {}", world.emissiveLastIndex);
        Log::info("Total triangles: {}", world.triangles.size());

        world.create();
        return world;
    }
};
