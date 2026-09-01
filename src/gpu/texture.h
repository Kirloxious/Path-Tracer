#pragma once

/**
 * @file texture.h
 * @brief RAII wrapper around an immutable-storage GL 2D texture.
 */

#include <glad/glad.h>

/**
 * @brief Owns one immutable-storage `GL_TEXTURE_2D`, usable as a sampler or a compute image.
 *
 * Single mip level, `GL_CLAMP_TO_EDGE` on both axes. Construction failure (non-positive
 * dimensions, or a null pixel pointer on the upload overload) is reported through Log::error
 * and leaves `handle` at 0 rather than throwing.
 *
 * Non-copyable, movable — a moved-from Texture has `handle == 0` and destroys nothing.
 */
class Texture
{
public:
    GLuint handle = 0;
    int    width = 0;
    int    height = 0;
    GLenum internalFormat = GL_RGBA32F;

    Texture() = default;

    /**
     * @brief Allocates storage and clears it to zero.
     *
     * Uses `GL_NEAREST` filtering — the intended use is `imageLoad`/`imageStore` from compute.
     * The zero-clear matters: without it an `imageLoad` on the accumulation target in frame 1
     * can return NaN/Inf on some drivers, and the running average propagates that forever.
     *
     * @param width          Texture width in texels; must be > 0.
     * @param height         Texture height in texels; must be > 0.
     * @param internalFormat Sized GL internal format (rgba32f by default).
     */
    Texture(int width, int height, GLenum internalFormat = GL_RGBA32F);

    /**
     * @brief Allocates storage and uploads a client-side pixel buffer into it.
     *
     * Uses `GL_LINEAR` filtering instead of `GL_NEAREST` — the caller for this overload is
     * the env-map loader, whose lookups want bilinear filtering.
     *
     * @param width          Texture width in texels; must be > 0.
     * @param height         Texture height in texels; must be > 0.
     * @param internalFormat Sized GL internal format for the storage.
     * @param pixelFormat    Client-data channel layout, per glTextureSubImage2D (e.g. GL_RGBA).
     * @param pixelType      Client-data component type, per glTextureSubImage2D (e.g. GL_FLOAT).
     * @param pixels         Row-major source pixels; must not be null.
     */
    Texture(int width, int height, GLenum internalFormat, GLenum pixelFormat, GLenum pixelType, const void* pixels);
    ~Texture();

    /// Binds this texture as image unit 0 with `GL_READ_WRITE` — the convention for the
    /// progressive-accumulation target.
    void bindForAccumulation() const;

    /**
     * @brief Binds this texture as a compute image unit.
     * @param unit   Image unit index, matching the shader's `layout(binding = ...)`.
     * @param access One of GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE.
     */
    void bind(int unit, GLenum access) const;

    // Non-copyable, movable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;
};
