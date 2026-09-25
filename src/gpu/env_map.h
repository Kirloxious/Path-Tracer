#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <filesystem>
#include <vector>

#include <glm/ext/vector_int2.hpp>

#include "gpu/texture.h"

/// pdfNumerator is the solid-angle pdf with sin(theta) factored out; the shader divides by the
/// sin of the drawn direction, not the cell centre. Mirrors `EnvSampleCell` in envmap.glsl.
struct alignas(16) EnvSampleCell
{
    float    accept = 1.0f;
    uint32_t alias = 0;
    float    pdfNumerator = 0.0f;
    float    _pad = 0.0f;
};

static_assert(sizeof(EnvSampleCell) == 16, "EnvSampleCell size must match std430 layout");
static_assert(offsetof(EnvSampleCell, alias) == 4);
static_assert(offsetof(EnvSampleCell, pdfNumerator) == 8);

class EnvMap
{
public:
    /// Default-constructs to an invalid map, meaning "no envmap".
    EnvMap() = default;

    static std::expected<EnvMap, std::string> load(const std::filesystem::path& hdrPath, float intensity);

    bool valid() const { return texture.id() != 0; }

    /// No-op when !valid().
    void bind(int unit) const;

    float getIntensity() const { return intensity; }

    /// Row-major over samplingSize(); empty when the map failed to load.
    const std::vector<EnvSampleCell>& samplingCells() const { return cells; }

    glm::ivec2 samplingSize() const { return sampleSize; }

private:
    void buildSamplingTable(const float* rgba, int width, int height);

    Texture                    texture;
    std::vector<EnvSampleCell> cells;
    glm::ivec2                 sampleSize{0, 0};
    float                      intensity = 1.0f;
};
