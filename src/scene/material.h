#pragma once

/**
 * @file material.h
 * @brief PBR material description shared verbatim with the shade kernels.
 */

#include <cmath>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>

/**
 * @brief Derived shading class. Selects which `shade_*.comp` kernel a hit is queued into.
 *
 * This is *not* authored — it is computed from the material parameters by Material::classify()
 * and cached in Material::type. Deriving it on the CPU keeps every routing decision on the GPU
 * an exact integer comparison, and confines the "how rough is specular?" thresholds to a single
 * function that can be logged and tuned, instead of scattering float comparisons across five
 * shaders where they can silently drift apart.
 *
 * Values must stay in sync with the `MAT_*` constants in the shaders. The numbering is
 * deliberately the same one the old authored MaterialType used, so the denoiser's edge-stop
 * thresholds, `resolve.comp`'s material-id write and the AOV albedo view keep working unchanged.
 */
enum class MaterialClass : uint32_t
{
    Diffuse = 0,      ///< Broad lobe. The only class ReSTIR will anchor a reservoir on.
    Specular = 1,     ///< Metal, or a smooth dielectric coat. Near-deterministic scatter.
    Transmissive = 2, ///< Refractive; `transmission` > 0.
    Emissive = 3,     ///< Emits light. Gathered into a LightGroup for NEE; paths terminate here.
};

/**
 * @brief One material, laid out to match the std430 `Material` in `primitives.glsl`.
 *
 * `alignas(16)`, 48 bytes; uploaded once per scene into the MatsBuffer at binding 1.
 * Metallic-roughness parameterization, matching glTF and Blender's Principled core:
 *
 *   - `roughness` is *perceptual* — the GGX width is alpha = roughness^2. This is what makes a
 *     linear slider feel linear, and it is the convention glTF assets are authored against.
 *   - `ior` drives the dielectric Fresnel term, F0 = ((ior-1)/(ior+1))^2. The 1.5 default gives
 *     the standard 0.04.
 *   - `base_color` is albedo for dielectrics and the F0 tint for conductors.
 *
 * Construct via the static factories, which set the parameters *and* refresh the cached `type`.
 * Assigning fields directly is supported (the scene editor does exactly that) but the caller
 * must then call refreshType() before the material is uploaded or used to sort emissives.
 */
struct alignas(16) Material
{
    glm::vec3 base_color = glm::vec3(1.0f); ///< Albedo (dielectric) or F0 tint (conductor).
    float     metallic = 0.0f;              ///< 0 = dielectric, 1 = conductor.

    glm::vec3 emission = glm::vec3(0.0f); ///< Radiance emitted per unit area; non-zero makes this a light.
    float     roughness = 1.0f;           ///< Perceptual roughness; GGX alpha = roughness^2. 0 = perfect mirror.

    float    ior = 1.5f;          ///< Index of refraction (1.5 = glass). Drives dielectric F0.
    float    transmission = 0.0f; ///< 0 = opaque, 1 = fully refractive.
    uint32_t type = 0u;           ///< Cached MaterialClass — derived, see classify().
    float    _pad0 = 0.0f;

    /// Roughness at or below which a surface is treated as specular rather than diffuse.
    /// Only consulted for non-metals; metals are always specular. Tuning this trades ReSTIR
    /// coverage (it only anchors on Diffuse) against the accuracy of its Lambertian target pdf.
    static constexpr float specularRoughnessMax = 0.08f;

    /// @return true when this material emits light and so participates in NEE / ReSTIR.
    ///         Reads `emission` directly rather than the cached `type`, so it is correct even
    ///         before refreshType() has run.
    [[nodiscard]] bool isEmissive() const { return emission.x > 0.0f || emission.y > 0.0f || emission.z > 0.0f; }

    /**
     * @brief Derives the shading class from the parameters.
     *
     * Order matters: emission wins over everything (a glowing pane of glass is still a light as
     * far as NEE is concerned), then transmission, then the metal/smoothness test.
     */
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

    /// Recomputes the cached `type`. Call after mutating any parameter by hand.
    void refreshType() { type = static_cast<uint32_t>(classify()); }

    /**
     * @brief Diffuse material with a Lambertian BSDF.
     * @param color Albedo in linear space, per channel in [0, 1].
     */
    [[nodiscard]] static Material Lambertian(glm::vec3 color) {
        Material m;
        m.base_color = color;
        m.metallic = 0.0f;
        m.roughness = 1.0f;
        m.refreshType();
        return m;
    }

    /**
     * @brief Specular reflector, optionally roughened.
     * @param color Reflectance tint in linear space.
     * @param fuzz  Legacy reflection-cone radius. Stored as `roughness = sqrt(fuzz)` so that
     *              `roughness^2` recovers it exactly — that quantity is the GGX alpha, so the
     *              mapping is the real parameterization rather than a compatibility shim.
     */
    [[nodiscard]] static Material Metal(glm::vec3 color, float fuzz) {
        Material m;
        m.base_color = color;
        m.metallic = 1.0f;
        m.roughness = std::sqrt(fuzz);
        m.refreshType();
        return m;
    }

    /**
     * @brief Smooth refractive material with Schlick-approximated Fresnel.
     *
     * Named for what it builds rather than for `metallic == 0`: in metallic-roughness terms
     * every non-conductor is a "dielectric", including each diffuse wall, so that word does
     * not distinguish this material. What sets it apart is `transmission`.
     *
     * @param refractive_index Index of refraction relative to air (1.5 ~ glass, 1.33 ~ water).
     */
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

    /**
     * @brief Frosted / rough refractive material.
     *
     * Same interface as Glass(), but the microfacet normal is drawn from the GGX visible-normal
     * distribution instead of being the surface normal, so both the reflected and refracted
     * lobes spread out.
     *
     * @param refractive_index Index of refraction relative to air.
     * @param roughness        Perceptual roughness in [0, 1]. At (or very near) 0 this is
     *                         identical to Glass().
     * @param tint             Multiplied into whatever passes through; white leaves it colourless.
     */
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

    /**
     * @brief Area-light material. Triangles using it are gathered into a LightGroup for NEE.
     * @param color    Tint applied to `emission`, and the albedo a scatter off the emitter would use.
     * @param emission Emitted radiance; values well above 1 are normal for a light.
     */
    [[nodiscard]] static Material Emissive(glm::vec3 color, glm::vec3 emission) {
        Material m;
        m.base_color = color;
        m.emission = emission;
        m.metallic = 0.0f;
        m.roughness = 1.0f;
        m.refreshType();
        return m;
    }

    /**
     * @brief Full metallic-roughness material.
     * @param base_color   Albedo (dielectric) or F0 tint (conductor).
     * @param metallic     0 = dielectric, 1 = conductor.
     * @param roughness    Perceptual roughness in [0, 1].
     * @param ior          Index of refraction; drives dielectric F0.
     * @param transmission 0 = opaque, 1 = fully refractive.
     */
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
