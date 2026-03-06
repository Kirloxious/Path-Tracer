#pragma once

#include <cstdint>
#include <glm/ext/vector_float3.hpp>

enum class MaterialType : uint32_t
{
    Lambertian = 0,
    Metal = 1,
    Dielectric = 2,
    Emissive = 3,
};

struct alignas(16) Material
{
    glm::vec3 color = glm::vec3(1.0f);
    float     fuzz = 0.0f;
    glm::vec3 emission = glm::vec3(0.0f);
    float     refractive_index = 0.0f;
    float     type = 0.0f;

    [[nodiscard]] bool isEmissive() const { return type == static_cast<float>(MaterialType::Emissive); }

    [[nodiscard]] static Material Lambertian(glm::vec3 color) {
        Material m;
        m.color = color;
        m.type = static_cast<float>(MaterialType::Lambertian);
        return m;
    }

    [[nodiscard]] static Material Metal(glm::vec3 color, float fuzz) {
        Material m;
        m.color = color;
        m.fuzz = fuzz;
        m.type = static_cast<float>(MaterialType::Metal);
        return m;
    }

    [[nodiscard]] static Material Dielectric(float refractive_index) {
        Material m;
        m.refractive_index = refractive_index;
        m.type = static_cast<float>(MaterialType::Dielectric);
        return m;
    }

    [[nodiscard]] static Material Emissive(glm::vec3 color, glm::vec3 emission) {
        Material m;
        m.color = color;
        m.emission = emission;
        m.type = static_cast<float>(MaterialType::Emissive);
        return m;
    }
};
