#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>

#include "log.h"

#include "bvh.h"
#include "utils.h"
#include "material.h"

class World
{
private:
    void create() { bvh.build(spheres); }

public:
    std::vector<Sphere>   spheres;
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

    void addQuad(glm::vec3 corner_point, glm::vec3 u, glm::vec3 v, uint32_t material_index) {
        quads.emplace_back(corner_point, u, v, material_index);
    }

    void addQuad(glm::vec3 corner_point, glm::vec3 u, glm::vec3 v, Material mat) { quads.emplace_back(corner_point, u, v, addMaterial(mat)); }

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

        // world.addSphere(glm::vec3(0.0f, 100.0f, 0.0f), 30.0f, Material::Emissive(glm::vec3(1.0f),
        // glm::vec3(15.0f)));

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
                    world.addSphere(center, 0.2f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(30.0f, 6.0f, 2.0f)));

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
        world.addSphere(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(30.0f, 6.0f, 2.0f)));

        // Red light
        world.addSphere(glm::vec3(-8.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(30.0f, 6.0f, 2.0f)));

        world.emissiveLastIndex = world.sortEmissiveFirst();

        Log::info("Emissive last index: {}", world.emissiveLastIndex);

        world.create();

        return world;
    }
};
