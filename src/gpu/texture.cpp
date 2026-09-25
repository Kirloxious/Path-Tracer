#include "gpu/texture.h"

#include "core/log.h"

Texture::Texture(int width, int height, GLenum internalFormat) : width(width), height(height), internalFormat(internalFormat) {
    if (width <= 0 || height <= 0) {
        Log::error("Texture created with invalid dimensions: {} x {}", width, height);
        return;
    }
    GLuint handle = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &handle);
    m_handle.reset(handle);
    glTextureStorage2D(handle, 1, internalFormat, width, height);
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Without this, frame-1 imageLoad on the accumulation target can return NaN/Inf on some
    // drivers, which the running average then propagates forever.
    const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLenum      format = GL_RGBA;
    switch (internalFormat) {
    case GL_RG16F:
    case GL_RG32F:
        format = GL_RG;
        break;
    case GL_R16F:
    case GL_R32F:
        format = GL_RED;
        break;
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
        format = GL_DEPTH_COMPONENT;
        break;
    default:
        break;
    }
    glClearTexImage(handle, 0, format, GL_FLOAT, zero);
}

Texture::Texture(int width, int height, GLenum internalFormat, GLenum pixelFormat, GLenum pixelType, const void* pixels)
    : width(width), height(height), internalFormat(internalFormat) {
    if (width <= 0 || height <= 0 || pixels == nullptr) {
        Log::error("Texture: bad upload ({}x{}, pixels={})", width, height, pixels);
        return;
    }
    GLuint handle = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &handle);
    m_handle.reset(handle);
    glTextureStorage2D(handle, 1, internalFormat, width, height);
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureSubImage2D(handle, 0, 0, 0, width, height, pixelFormat, pixelType, pixels);
}

void Texture::bindForAccumulation() const {
    glBindImageTexture(0, id(), 0, GL_FALSE, 0, GL_READ_WRITE, internalFormat);
}

void Texture::bindSampler(int unit) const {
    glBindTextureUnit(static_cast<GLuint>(unit), id());
}

void Texture::setFilter(GLenum filter) const {
    glTextureParameteri(id(), GL_TEXTURE_MIN_FILTER, static_cast<GLint>(filter));
    glTextureParameteri(id(), GL_TEXTURE_MAG_FILTER, static_cast<GLint>(filter));
}

void Texture::copyTo(const Texture& dst) const {
    glCopyImageSubData(id(), GL_TEXTURE_2D, 0, 0, 0, 0, dst.id(), GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
}

void Texture::bind(int unit, GLenum access) const {
    glBindImageTexture(unit, id(), 0, GL_FALSE, 0, access, internalFormat);
}
