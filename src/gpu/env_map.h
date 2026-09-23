#pragma once

/**
 * @file env_map.h
 * @brief Equirectangular HDR environment map used as a distant area light.
 */

#include <cstdint>
#include <filesystem>
#include <vector>

#include <glm/ext/vector_int2.hpp>

#include "gpu/texture.h"

/**
 * @brief One cell of the environment map's importance-sampling grid.
 *
 * A Vose alias table over a downsampled copy of the map, so drawing a cell proportional to its
 * radiance costs one load and one compare instead of a CDF binary search.
 *
 * `pdfNumerator` is the solid-angle pdf with the equirect Jacobian's `sin(theta)` factored
 * out. The shader divides by `sin(theta)` of the direction it actually drew rather than of the
 * cell centre — the cell spans a range of theta, and using the centre would make the density
 * disagree with the sampling procedure by a few percent near the poles.
 *
 * Mirrors `EnvSampleCell` in `shader/common/envmap.glsl`.
 */
struct alignas(16) EnvSampleCell
{
    float    accept = 1.0f;       ///< Alias acceptance probability for this cell.
    uint32_t alias = 0;           ///< Cell taken when the acceptance test fails.
    float    pdfNumerator = 0.0f; ///< Solid-angle pdf times sin(theta).
    float    _pad = 0.0f;
};

static_assert(sizeof(EnvSampleCell) == 16, "EnvSampleCell size must match std430 layout");

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

    /// @return The importance-sampling alias table, row-major over `samplingSize()`. Empty
    ///         when the map failed to load.
    const std::vector<EnvSampleCell>& samplingCells() const { return cells; }

    /// @return Dimensions of the sampling grid, which is a downsampled copy of the source.
    glm::ivec2 samplingSize() const { return sampleSize; }

    // Non-copyable, movable (mirrors Texture wrapper).
    EnvMap(const EnvMap&) = delete;
    EnvMap& operator=(const EnvMap&) = delete;
    EnvMap(EnvMap&&) noexcept = default;
    EnvMap& operator=(EnvMap&&) noexcept = default;

private:
    /// Box-averages the source into the sampling grid and builds the alias table over it.
    void buildSamplingTable(const float* rgb, int width, int height);

    Texture                    texture;
    std::vector<EnvSampleCell> cells;
    glm::ivec2                 sampleSize{0, 0};
    float                      intensity = 1.0f;
};
