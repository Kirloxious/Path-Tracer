#include "gpu/env_map.h"

// stb_image is header-only. IMPLEMENTATION goes exactly here — mirrors the
// tinyobjloader convention documented in CLAUDE.md.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vector>

#include "core/log.h"

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
}

void EnvMap::bind(int unit) const {
    if (!valid()) return;
    glBindTextureUnit(static_cast<GLuint>(unit), texture.handle);
}
