#pragma once

#include <cstddef>
#include <cmath>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>

#include "core/shader_shared.h"

/// Derived by Material::classify(), never authored, so GPU routing stays an exact integer compare
/// and the roughness thresholds live in one function. Values are the `MAT_*` defines.
enum class MaterialClass : uint32_t
{
    Diffuse = MAT_DIFFUSE,
    Specular = MAT_SPECULAR,
    Transmissive = MAT_TRANSMISSIVE,
    Emissive = MAT_EMISSIVE,
};

/// Mirrors std430 `Material` in primitives.glsl. After assigning fields directly, call refreshType()
/// before upload or emissive sorting.
struct alignas(16) Material
{
    glm::vec3 base_color = glm::vec3(1.0f); ///< Albedo (dielectric) or F0 tint (conductor).
    float     metallic = 0.0f;

    glm::vec3 emission = glm::vec3(0.0f); ///< Tinted by `base_color`; see emittedRadiance().
    float     roughness = 1.0f;           ///< Perceptual; GGX alpha = roughness^2.

    float    ior = 1.5f;
    float    transmission = 0.0f;
    uint32_t type = 0u; ///< Derived; see classify().
    float    _pad0 = 0.0f;

    /// Non-metals at or below this roughness are Specular. Trades ReSTIR coverage (it anchors on Diffuse)
    /// against the accuracy of its Lambertian target pdf.
    static constexpr float specularRoughnessMax = 0.08f;

    /// Mirrors material_emission() in primitives.glsl.
    [[nodiscard]] glm::vec3 emittedRadiance() const { return base_color * emission; }

    /// Derived from the parameters, not `type`, so it is correct before refreshType() has run.
    [[nodiscard]] bool isEmissive() const {
        const glm::vec3 le = emittedRadiance();
        return le.x > 0.0f || le.y > 0.0f || le.z > 0.0f;
    }

    /// Order matters: emission wins (glowing glass is still a light to NEE), then transmission.
    [[nodiscard]] MaterialClass classify() const {
        if (isEmissive()) {
            return MaterialClass::Emissive;
        }
        if (transmission > 0.0f) {
            return MaterialClass::Transmissive;
        }
        if (metallic > 0.5f || roughness <= specularRoughnessMax) {
            return MaterialClass::Specular;
        }
        return MaterialClass::Diffuse;
    }

    /// Call after mutating any parameter by hand.
    void refreshType() { type = static_cast<uint32_t>(classify()); }

    [[nodiscard]] static Material Lambertian(glm::vec3 color) {
        Material m;
        m.base_color = color;
        m.metallic = 0.0f;
        m.roughness = 1.0f;
        m.refreshType();
        return m;
    }

    /// @param fuzz Stored as roughness = sqrt(fuzz), so fuzz is exactly the GGX alpha.
    [[nodiscard]] static Material Metal(glm::vec3 color, float fuzz) {
        Material m;
        m.base_color = color;
        m.metallic = 1.0f;
        m.roughness = std::sqrt(fuzz);
        m.refreshType();
        return m;
    }

    [[nodiscard]] static Material Glass(float refractive_index) {
        Material m;
        m.base_color = glm::vec3(1.0f);
        m.metallic = 0.0f;
        m.roughness = 0.0f;
        m.ior = refractive_index;
        m.transmission = 1.0f;
        m.refreshType();
        return m;
    }

    [[nodiscard]] static Material RoughGlass(float refractive_index, float roughness, glm::vec3 tint = glm::vec3(1.0f)) {
        Material m;
        m.base_color = tint;
        m.metallic = 0.0f;
        m.roughness = roughness;
        m.ior = refractive_index;
        m.transmission = 1.0f;
        m.refreshType();
        return m;
    }

    /// @param color Tints `emission` and doubles as the albedo for a scatter off the emitter.
    [[nodiscard]] static Material Emissive(glm::vec3 color, glm::vec3 emission) {
        Material m;
        m.base_color = color;
        m.emission = emission;
        m.metallic = 0.0f;
        m.roughness = 1.0f;
        m.refreshType();
        return m;
    }

    [[nodiscard]] static Material Principled(glm::vec3 base_color, float metallic, float roughness, float ior = 1.5f, float transmission = 0.0f) {
        Material m;
        m.base_color = base_color;
        m.metallic = metallic;
        m.roughness = roughness;
        m.ior = ior;
        m.transmission = transmission;
        m.refreshType();
        return m;
    }
};

static_assert(sizeof(Material) == 48, "Material must be 48 bytes for std430");
static_assert(offsetof(Material, metallic) == 12);
static_assert(offsetof(Material, emission) == 16);
static_assert(offsetof(Material, roughness) == 28);
static_assert(offsetof(Material, ior) == 32);
static_assert(offsetof(Material, transmission) == 36);
static_assert(offsetof(Material, type) == 40);
