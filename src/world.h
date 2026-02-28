#pragma once

#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <numeric>

#include "bvh.h"
#include "utils.h"

enum class MaterialType : uint32_t {
    Lambertian  = 0,
    Metal       = 1,
    Dielectric  = 2,
    Emissive    = 3,
};

struct alignas(16) Material {
    glm::vec3 color            = glm::vec3(1.0f);
    float     fuzz             = 0.0f;
    glm::vec3 emission         = glm::vec3(0.0f);
    float     refractive_index = 0.0f;
    float     type             = 0.0f;

    static Material Lambertian(glm::vec3 color) {
        Material m;
        m.color = color;
        m.type  = (float)MaterialType::Lambertian;
        return m;
    }

    static Material Metal(glm::vec3 color, float fuzz) {
        Material m;
        m.color = color;
        m.fuzz  = fuzz;
        m.type  = (float)MaterialType::Metal;
        return m;
    }

    static Material Dielectric(float refractive_index) {
        Material m;
        m.refractive_index = refractive_index;
        m.type             = (float)MaterialType::Dielectric;
        return m;
    }

    static Material Emissive(glm::vec3 color, glm::vec3 emission) {
        Material m;
        m.color    = color;
        m.emission = emission;
        m.type     = (float)MaterialType::Emissive;
        return m;
    }
};

class World
{
public:
    std::vector<Sphere>   spheres;
    std::vector<Material> materials;
    std::vector<BVHNodeFlat> bvh;
    int                      bvhRoot = -1;
    uint32_t bvhSize = 0;

    // Returns the index of the added material
    uint32_t addMaterial(Material mat) {
        materials.push_back(mat);
        return (uint32_t)materials.size() - 1;
    }

    void addSphere(glm::vec3 position, float radius, uint32_t material_index) {
        spheres.emplace_back(position, radius, material_index);
    }

    void addSphere(glm::vec3 position, float radius, Material mat) {
        addSphere(position, radius, addMaterial(mat));
    }

    void constructBVH(){
        std::vector<AABB> spheresAABBS;
        for (const auto& sphere : spheres) {
            spheresAABBS.push_back(computeAABB(sphere));
        }
        std::cout << "Number of spheres: " << spheres.size() << std::endl;

        std::vector<BVHNode> bvhNodes;

        std::vector<int> sphereIndices(spheres.size());
        std::iota(sphereIndices.begin(), sphereIndices.end(), 0); // [0, 1, 2, ..., N]
        
        bvhRoot = buildBVH(bvhNodes, spheresAABBS, sphereIndices);

        bvhSize = bvhNodes.size();
        bvh.reserve(bvhSize);
        flattenBVH(bvhRoot, bvhNodes, bvh, -1);
    }

    static void buildSphereWorld(World& world){
        // Ground sphere and mat
        uint32_t ground = world.addMaterial(Material::Lambertian(glm::vec3(0.5f, 0.5f, 0.5f)));
        world.addSphere(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, ground);

        for(int a = -11; a < 11; a++) {
            for(int b = -11; b < 11; b++) {
                float choose_mat = randomFloat();
                glm::vec3 center = glm::vec3(a + 0.9f * randomFloat(), 0.2f, b + 0.9f * randomFloat());
                if (choose_mat < 0.8f) {
                    // diffuse
                    glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                    world.addSphere(center, 0.2f, Material::Lambertian(color));
                }
                else if (choose_mat < 0.95f) {
                    // metal
                    glm::vec3 color = glm::vec3(randomFloat(), randomFloat(), randomFloat());
                    float fuzz = 0.5f * randomFloat();
                    world.addSphere(center, 0.2f, Material::Metal(color, fuzz));
                }
                else {
                    // glass
                    world.addSphere(center, 0.2f, Material::Dielectric(1.5f));
                }
            }
        }
        
        // Big spheres
        
        //Glasss
        world.addSphere(glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, Material::Dielectric(1.5f));
        //Mirror
        world.addSphere(glm::vec3(-4.0f, 1.0f, 0.0f), 1.0f, Material::Lambertian(glm::vec3(0.4f, 0.2f, 0.1f)));
        //Brown
        world.addSphere(glm::vec3(4.0f, 1.0f, 0.0f), 1.0f,Material::Metal(glm::vec3(0.7f, 0.6f, 0.5f), 0.0f));
        //Red light
        world.addSphere(glm::vec3(-8.0f, 1.0f, 0.0f), 1.0f, Material::Emissive(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(15.0f, 6.0f, 2.0f)));
        

        world.constructBVH();
    
    }
};


