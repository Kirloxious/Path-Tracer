#pragma once

/**
 * @file material.h
 * @brief BSDF material description shared verbatim with the shade kernels.
 */

#include <cstdint>
#include <glm/ext/vector_float3.hpp>

/**
 * @brief Material kind. Selects which `shade_*.comp` kernel a hit is queued into.
 *
 * Values must stay in sync with the `MAT_*` constants in the shaders.
 */
enum class MaterialType : uint32_t
{
    Lambertian = 0,
    Metal = 1,
    Dielectric = 2,
    Emissive = 3,
};

/**
 * @brief One material, laid out to match the std430 `Material` in `scene_buffers.glsl`.
 *
 * `alignas(16)`; uploaded once per scene into the MatsBuffer at binding 1. Only the fields
 * relevant to `type` are meaningful — the static factories leave the rest at their defaults.
 * Construct via the factories rather than filling fields directly, so `type` can never drift
 * from the populated parameters.
 */
struct alignas(16) Material
{
    glm::vec3 color = glm::vec3(1.0f);    ///< Albedo (Lambertian/Metal) or emitter tint.
    float     fuzz = 0.0f;                ///< Metal roughness: reflection-lobe cone radius.
    glm::vec3 emission = glm::vec3(0.0f); ///< Radiance emitted per unit area; non-zero only for Emissive.
    float     refractive_index = 0.0f;    ///< Dielectric index of refraction (e.g. 1.5 for glass).
    uint32_t  type = 0u;                  ///< A MaterialType value.

    /// @return true when this material emits light and so participates in NEE / ReSTIR.
    [[nodiscard]] bool isEmissive() const { return type == static_cast<uint32_t>(MaterialType::Emissive); }

    /**
     * @brief Diffuse material with a cosine-weighted BSDF.
     * @param color Albedo in linear space, per channel in [0, 1].
     */
    [[nodiscard]] static Material Lambertian(glm::vec3 color) {
        Material m;
        m.color = color;
        m.type = static_cast<uint32_t>(MaterialType::Lambertian);
        return m;
    }

    /**
     * @brief Specular reflector, optionally roughened.
     * @param color Reflectance tint in linear space.
     * @param fuzz  Perturbation radius applied to the mirror direction. 0 is a perfect mirror;
     *              larger values blur the reflection.
     */
    [[nodiscard]] static Material Metal(glm::vec3 color, float fuzz) {
        Material m;
        m.color = color;
        m.fuzz = fuzz;
        m.type = static_cast<uint32_t>(MaterialType::Metal);
        return m;
    }

    /**
     * @brief Smooth refractive material with Schlick-approximated Fresnel.
     * @param refractive_index Index of refraction relative to air (1.5 ≈ glass, 1.33 ≈ water).
     */
    [[nodiscard]] static Material Dielectric(float refractive_index) {
        Material m;
        m.refractive_index = refractive_index;
        m.type = static_cast<uint32_t>(MaterialType::Dielectric);
        return m;
    }

    /**
     * @brief Area-light material. Triangles using it are gathered into a LightGroup for NEE.
     * @param color    Albedo used when a path scatters off the emitter rather than terminating.
     * @param emission Emitted radiance; values well above 1 are normal for a light.
     */
    [[nodiscard]] static Material Emissive(glm::vec3 color, glm::vec3 emission) {
        Material m;
        m.color = color;
        m.emission = emission;
        m.type = static_cast<uint32_t>(MaterialType::Emissive);
        return m;
    }
};
