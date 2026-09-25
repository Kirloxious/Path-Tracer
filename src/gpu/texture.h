#pragma once

#include <glad/glad.h>

#include "gpu/gl_handle.h"

/// Single mip, CLAMP_TO_EDGE. Construction failure is logged and leaves id() at 0.
class Texture
{
public:
    int    width = 0;
    int    height = 0;
    GLenum internalFormat = GL_RGBA32F;

    Texture() = default;

    /// Zero-initialised, GL_NEAREST filtering.
    Texture(int width, int height, GLenum internalFormat = GL_RGBA32F);

    /// GL_LINEAR filtering, for the env map's bilinear lookups.
    Texture(int width, int height, GLenum internalFormat, GLenum pixelFormat, GLenum pixelType, const void* pixels);

    void bindForAccumulation() const;

    void bind(int unit, GLenum access) const;

    void bindSampler(int unit) const;

    void setFilter(GLenum filter) const;

    /// @p dst must have the same size and a compatible format.
    void copyTo(const Texture& dst) const;

    [[nodiscard]] GLuint id() const { return m_handle.get(); }

private:
    TextureHandle m_handle;
};
