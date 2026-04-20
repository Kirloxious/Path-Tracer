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
    ~Texture();

    void bindForAccumulation() const;
    void bind(int unit, GLenum access) const;

    // Non-copyable, movable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;
};
