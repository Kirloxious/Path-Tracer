#pragma once



#include <cstdint>
#include <glm/ext/vector_float3.hpp>

struct alignas(16) Sphere
{
    glm::vec3 position       = glm::vec3(0.0f);
    float     radius         = 1.0f;
    uint32_t  material_index = 0;

    Sphere() = default;
    Sphere(glm::vec3 position, float radius, uint32_t material_index)
        : position(position), radius(radius), material_index(material_index) {}
};

struct alignas(16) Quad
{
    glm::vec3 corner_point;
    glm::vec3 u;
    glm::vec3 v;
    uint32_t material_index;

    Quad() = default;
    Quad(glm::vec3 corner_point, glm::vec3 u, glm::vec3 v, uint32_t material_index)
        : corner_point(corner_point), u(u), v(v), material_index(material_index) {}
};