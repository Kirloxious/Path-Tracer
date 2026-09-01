#pragma once

/**
 * @file env_map.h
 * @brief Equirectangular HDR environment map used as a distant area light.
 */

#include <filesystem>

#include "gpu/texture.h"

/**
 * @brief An equirectangular HDR environment map, owned as an rgba32f GL texture.
 *
 * Missed rays sample it as a distant light — primary sky in `generate.comp`, secondary miss
 * in `trace.comp`. There is no NEE toward the environment, so convergence depends heavily on
 * the map: an overcast or soft HDR converges much faster than a sunny one with a small bright
 * sun disc.
 *
 * Load failures are logged and leave the object invalid (valid() == false) rather than
 * throwing, so a scene with a missing HDR still renders — just with a black sky.
 *
 * Non-copyable, movable (mirrors the Texture wrapper).
 */
class EnvMap
{
public:
    /// Default-constructs to an invalid map — the representation of "this scene has no envmap".
    EnvMap() = default;

    /**
     * @brief Loads a `.hdr` via stb_image and uploads it as an rgba32f texture.
     *
     * The source is read as 3-channel float and widened to RGBA on the CPU before upload.
     *
     * @param hdrPath   Path to an equirectangular radiance-HDR file.
     * @param intensity Multiplier applied to the sampled radiance by the shaders.
     */
    EnvMap(const std::filesystem::path& hdrPath, float intensity);

    /// @return true when a texture was successfully loaded and can be bound.
    bool valid() const { return texture.handle != 0; }

    /**
     * @brief Binds the map to a sampler texture unit. No-op when !valid().
     * @param unit Texture unit index matching the shader's sampler binding.
     */
    void bind(int unit) const;

    /// @return The radiance multiplier passed at construction.
    float getIntensity() const { return intensity; }

    // Non-copyable, movable (mirrors Texture wrapper).
    EnvMap(const EnvMap&) = delete;
    EnvMap& operator=(const EnvMap&) = delete;
    EnvMap(EnvMap&&) noexcept = default;
    EnvMap& operator=(EnvMap&&) noexcept = default;

private:
    Texture texture;
    float   intensity = 1.0f;
};
