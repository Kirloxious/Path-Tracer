#include "gpu/env_map.h"

// Exactly one TU may define the stb_image implementation.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <vector>

#include "core/log.h"
#include "core/alias_table.h"

namespace {
/// Only has to locate the energy for the alias draw; radiance is still fetched from the
/// full-resolution texture, so a coarse grid costs pdf accuracy, not detail.
constexpr int ENV_SAMPLE_MAX_W = 1024;
constexpr int ENV_SAMPLE_MAX_H = 512;
} // namespace

std::expected<EnvMap, std::string> EnvMap::load(const std::filesystem::path& hdrPath, float intensity) {
    int    w = 0, h = 0, n = 0;
    float* stbPixels = stbi_loadf(hdrPath.string().c_str(), &w, &h, &n, 3);
    if (!stbPixels) {
        return std::unexpected(std::format("EnvMap: failed to load HDR '{}' ({})", hdrPath.string(), stbi_failure_reason()));
    }
    Log::info("EnvMap: loaded {} — {}x{} (source channels={})", hdrPath.filename().string(), w, h, n);

    std::vector<float> rgba(static_cast<size_t>(w) * h * 4);
    for (int i = 0; i < w * h; ++i) {
        rgba[4 * i + 0] = stbPixels[3 * i + 0];
        rgba[4 * i + 1] = stbPixels[3 * i + 1];
        rgba[4 * i + 2] = stbPixels[3 * i + 2];
        rgba[4 * i + 3] = 1.0f;
    }
    stbi_image_free(stbPixels);

    EnvMap map;
    map.intensity = intensity;
    map.texture = Texture(w, h, GL_RGBA32F, GL_RGBA, GL_FLOAT, rgba.data());
    map.buildSamplingTable(rgba.data(), w, h);
    return map;
}

void EnvMap::buildSamplingTable(const float* rgba, int w, int h) {
    constexpr float pi = std::numbers::pi_v<float>;

    sampleSize.x = std::min(w, ENV_SAMPLE_MAX_W);
    sampleSize.y = std::min(h, ENV_SAMPLE_MAX_H);
    const int sw = sampleSize.x;
    const int sh = sampleSize.y;
    const int n = sw * sh;

    std::vector<float> weight(static_cast<std::size_t>(n), 0.0f);
    double             total = 0.0;

    for (int y = 0; y < sh; ++y) {
        const int y0 = y * h / sh;
        const int y1 = std::max(y0 + 1, (y + 1) * h / sh);
        // Equirect Jacobian: without it the poles, which cover almost no solid angle, attract as
        // many samples as the horizon.
        const float sinTheta = std::sin((static_cast<float>(y) + 0.5f) / static_cast<float>(sh) * pi);

        for (int x = 0; x < sw; ++x) {
            const int x0 = x * w / sw;
            const int x1 = std::max(x0 + 1, (x + 1) * w / sw);

            // Box-average, not a point sample: a sun smaller than one cell must still put its energy
            // into that cell or the sampler never finds it.
            double sum = 0.0;
            for (int yy = y0; yy < y1; ++yy) {
                for (int xx = x0; xx < x1; ++xx) {
                    const float* p = rgba + 4 * (static_cast<std::size_t>(yy) * w + xx);
                    sum += 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
                }
            }
            const float lum = static_cast<float>(sum / ((y1 - y0) * (x1 - x0)));
            weight[static_cast<std::size_t>(y) * sw + x] = lum * sinTheta;
            total += weight[static_cast<std::size_t>(y) * sw + x];
        }
    }

    // All-black map: the Jacobian weight alone gives a uniform distribution over the sphere.
    if (!(total > 0.0)) {
        total = 0.0;
        for (int y = 0; y < sh; ++y) {
            const float sinTheta = std::sin((static_cast<float>(y) + 0.5f) / static_cast<float>(sh) * pi);
            for (int x = 0; x < sw; ++x) {
                weight[static_cast<std::size_t>(y) * sw + x] = sinTheta;
                total += sinTheta;
            }
        }
    }

    const AliasTable table = buildAliasTable(weight);

    // p(omega) = p_cell * sw * sh / (2 pi^2 sin(theta)); everything but the sin is per-cell.
    const double pdfScale = static_cast<double>(sw) * sh / (total * 2.0 * pi * pi);

    cells.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        cells[i].accept = std::clamp(table.accept[i], 0.0f, 1.0f);
        cells[i].alias = table.alias[i];
        cells[i].pdfNumerator = static_cast<float>(weight[i] * pdfScale);
    }

    Log::info("EnvMap: sampling grid {}x{} ({:.1f} KB alias table)", sw, sh, n * sizeof(EnvSampleCell) / 1024.0);
}

void EnvMap::bind(int unit) const {
    if (!valid()) {
        return;
    }
    texture.bindSampler(unit);
}
