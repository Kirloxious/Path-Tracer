#pragma once

/**
 * @file env_map.h
 * @brief Equirectangular HDR environment map used as a distant area light.
 */

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
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
static_assert(offsetof(EnvSampleCell, alias) == 4);
static_assert(offsetof(EnvSampleCell, pdfNumerator) == 8);

/**
 * @brief An equirectangular HDR environment map, owned as an rgba32f GL texture.
 *
 * Missed rays read it as a distant light (primary sky in `generate.comp`, secondary miss in
 * `trace.comp`), and shade_surface's NEE importance-samples it through the alias table built
 * in buildSamplingTable().
 *
 * Move-only.
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
     * @return The loaded map, or a description of why the file could not be read.
     */
    static std::expected<EnvMap, std::string> load(const std::filesystem::path& hdrPath, float intensity);

    /// @return true when a texture was successfully loaded and can be bound.
    bool valid() const { return texture.id() != 0; }

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

private:
    /// Box-averages the source into the sampling grid and builds the alias table over it.
    void buildSamplingTable(const float* rgba, int width, int height);

    Texture                    texture;
    std::vector<EnvSampleCell> cells;
    glm::ivec2                 sampleSize{0, 0};
    float                      intensity = 1.0f;
};
