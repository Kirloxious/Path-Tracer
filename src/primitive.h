#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct alignas(16) Triangle
{
    glm::vec3 v0 = glm::vec3(0.0f);
    float     area = 0.0f;          // 0.5 * |e1 x e2| — used by NEE light-pdf math
    glm::vec3 e1 = glm::vec3(0.0f); // v1 - v0
    float     _pad1 = 0.0f;
    glm::vec3 e2 = glm::vec3(0.0f); // v2 - v0
    uint32_t  material_index = 0;
    glm::vec3 n0 = glm::vec3(0.0f); // vertex normal at v0
    float     _pad2 = 0.0f;
    glm::vec3 n1 = glm::vec3(0.0f); // vertex normal at v1
    float     _pad3 = 0.0f;
    glm::vec3 n2 = glm::vec3(0.0f); // vertex normal at v2
    float     _pad4 = 0.0f;

    Triangle() = default;

    // Flat shading: all vertex normals set to face normal
    Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index)
        : v0(v0), area(0.5f * glm::length(glm::cross(v1 - v0, v2 - v0))), e1(v1 - v0), e2(v2 - v0), material_index(material_index) {
        glm::vec3 fn = glm::normalize(glm::cross(e1, e2));
        n0 = n1 = n2 = fn;
    }

    // Smooth shading: per-vertex normals provided
    Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index, glm::vec3 n0, glm::vec3 n1, glm::vec3 n2)
        : v0(v0), area(0.5f * glm::length(glm::cross(v1 - v0, v2 - v0))), e1(v1 - v0), e2(v2 - v0), material_index(material_index), n0(n0), n1(n1),
          n2(n2) {}
};

struct alignas(16) Quad
{
    glm::vec3 corner_point;
    glm::vec3 u;
    glm::vec3 v;
    uint32_t  material_index;

    Quad() = default;
    Quad(glm::vec3 corner_point, glm::vec3 u, glm::vec3 v, uint32_t material_index)
        : corner_point(corner_point), u(u), v(v), material_index(material_index) {}
};