#include "gpu/env_map.h"

// stb_image is header-only. IMPLEMENTATION goes exactly here — mirrors the
// tinyobjloader convention documented in CLAUDE.md.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "core/log.h"

namespace {
/// Sampling-grid ceiling. The grid only has to resolve *where* the energy is well enough for
/// the alias draw to find it; the radiance itself is still fetched from the full-resolution
/// texture at the jittered direction, so a coarse grid costs accuracy in the pdf, not detail
/// in the light. 1024x512 is 8 MB of table, against 64 MB for a 4K source at full resolution.
constexpr int ENV_SAMPLE_MAX_W = 1024;
constexpr int ENV_SAMPLE_MAX_H = 512;
} // namespace

EnvMap::EnvMap(const std::filesystem::path& hdrPath, float intensity) : intensity(intensity) {
    int w = 0, h = 0, n = 0;
    // Request 3 channels — HDR files are usually RGB (RGBE decoded); we widen to RGBA below.
    float* stbPixels = stbi_loadf(hdrPath.string().c_str(), &w, &h, &n, 3);
    if (!stbPixels) {
        Log::error("EnvMap: failed to load HDR '{}' ({})", hdrPath.string(), stbi_failure_reason());
        return;
    }
    Log::info("EnvMap: loaded {} — {}x{} (source channels={})", hdrPath.filename().string(), w, h, n);

    // GL's texture upload with GL_RGB / GL_FLOAT works for RGBA32F storage, so we could
    // skip the widen. But a strided fetch in the shader is fine either way, and RGBA is
    // less alignment-fragile across drivers — do the widen once here.
    std::vector<float> rgba(static_cast<size_t>(w) * h * 4);
    for (int i = 0; i < w * h; ++i) {
        rgba[4 * i + 0] = stbPixels[3 * i + 0];
        rgba[4 * i + 1] = stbPixels[3 * i + 1];
        rgba[4 * i + 2] = stbPixels[3 * i + 2];
        rgba[4 * i + 3] = 1.0f;
    }
    stbi_image_free(stbPixels);

    texture = Texture(w, h, GL_RGBA32F, GL_RGBA, GL_FLOAT, rgba.data());
    buildSamplingTable(rgba.data(), w, h);
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
        // The equirect Jacobian. Without it the poles — which cover almost no solid angle —
        // attract as many samples as the horizon, and a bright sky gradient reads as noise
        // concentrated overhead.
        const float sinTheta = std::sin((static_cast<float>(y) + 0.5f) / static_cast<float>(sh) * pi);

        for (int x = 0; x < sw; ++x) {
            const int x0 = x * w / sw;
            const int x1 = std::max(x0 + 1, (x + 1) * w / sw);

            // Box-average, not a point sample: a sun smaller than one grid cell still has to
            // put its energy into that cell, or the sampler never finds it and the exact case
            // this table exists for stays noisy.
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

    // An all-black map still needs a valid distribution: fall back to uniform over the sphere,
    // which is what weighting by the Jacobian alone gives.
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

    // Vose alias construction, as in World::buildLightGroups: scaled probabilities have mean
    // exactly 1, so each slot is either under- or over-full and can be paired off.
    std::vector<float>    p(static_cast<std::size_t>(n));
    std::vector<uint32_t> alias(static_cast<std::size_t>(n), 0);
    std::vector<float>    accept(static_cast<std::size_t>(n), 1.0f);
    std::vector<int>      small, large;
    small.reserve(n);
    large.reserve(n);

    for (int i = 0; i < n; ++i) {
        p[i] = static_cast<float>(static_cast<double>(n) * weight[i] / total);
        (p[i] < 1.0f ? small : large).push_back(i);
    }
    while (!small.empty() && !large.empty()) {
        const int l = small.back();
        small.pop_back();
        const int g = large.back();
        large.pop_back();

        accept[l] = p[l];
        alias[l] = static_cast<uint32_t>(g);
        p[g] = (p[g] + p[l]) - 1.0f;
        (p[g] < 1.0f ? small : large).push_back(g);
    }
    for (const int i : large) {
        accept[i] = 1.0f;
    }
    for (const int i : small) {
        accept[i] = 1.0f;
    }

    // p(omega) = p_cell * sw * sh / (2 pi^2 sin(theta)); everything but the sin is per-cell.
    const double pdfScale = static_cast<double>(sw) * sh / (total * 2.0 * pi * pi);

    cells.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        cells[i].accept = std::clamp(accept[i], 0.0f, 1.0f);
        cells[i].alias = alias[i];
        cells[i].pdfNumerator = static_cast<float>(weight[i] * pdfScale);
    }

    Log::info("EnvMap: sampling grid {}x{} ({:.1f} KB alias table)", sw, sh, n * sizeof(EnvSampleCell) / 1024.0);
}

void EnvMap::bind(int unit) const {
    if (!valid()) {
        return;
    }
    glBindTextureUnit(static_cast<GLuint>(unit), texture.handle);
}
