#pragma once

#include <glad/glad.h>

class Texture
{
public:
    GLuint handle = 0;
    int    width = 0;
    int    height = 0;
    GLenum internalFormat = GL_RGBA32F;

    Texture() = default;
    Texture(int width, int height, GLenum internalFormat = GL_RGBA32F);
    // Upload a pixel buffer (client-side row-major) after storage allocation. `pixelFormat`
    // and `pixelType` match glTextureSubImage2D semantics (e.g. GL_RGB / GL_FLOAT for HDRs).
    // Uses LINEAR filtering instead of NEAREST — env-map lookups want bilinear.
    Texture(int width, int height, GLenum internalFormat, GLenum pixelFormat, GLenum pixelType, const void* pixels);
    ~Texture();

    void bindForAccumulation() const;
    void bind(int unit, GLenum access) const;

    // Non-copyable, movable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;
};
