#pragma once

#include <filesystem>

#include "gpu/texture.h"

// Equirectangular HDR environment map. Loaded from .hdr on construction via
// stb_image; owned as an rgba32f GL texture. Missed rays (primary sky in
// generate.comp, secondary miss in trace.comp) sample it as a distant area
// light — v1 uses miss-sampling only (no NEE toward the env), so an overcast
// or soft HDR converges much faster than a sunny one with a small bright disc.
class EnvMap
{
public:
    EnvMap() = default;
    EnvMap(const std::filesystem::path& hdrPath, float intensity);

    bool  valid() const { return texture.handle != 0; }
    void  bind(int unit) const;
    float getIntensity() const { return intensity; }

    // Non-copyable, movable (mirrors Texture wrapper).
    EnvMap(const EnvMap&)            = delete;
    EnvMap& operator=(const EnvMap&) = delete;
    EnvMap(EnvMap&&) noexcept        = default;
    EnvMap& operator=(EnvMap&&) noexcept = default;

private:
    Texture texture;
    float   intensity = 1.0f;
};
