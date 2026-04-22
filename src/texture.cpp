#include "texture.h"

#include "log.h"

Texture::Texture(int width, int height, GLenum internalFormat) : width(width), height(height), internalFormat(internalFormat) {
    if (width <= 0 || height <= 0) {
        Log::error("Texture created with invalid dimensions: {} x {}", width, height);
        return;
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &handle);
    glTextureStorage2D(handle, 1, internalFormat, width, height);
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Zero-initialise. Without this, imageLoad on the accumulation target in the first frame
    // can return NaN/Inf (driver-dependent), and the shader's `prev_color * 0` term on frame 1
    // propagates that into every subsequent frame.
    const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearTexImage(handle, 0, GL_RGBA, GL_FLOAT, zero);
}

Texture::~Texture() {
    if (handle) {
        glDeleteTextures(1, &handle);
    }
}

Texture::Texture(Texture&& o) noexcept : handle(o.handle), width(o.width), height(o.height), internalFormat(o.internalFormat) {
    o.handle = 0;
}

Texture& Texture::operator=(Texture&& o) noexcept {
    if (this != &o) {
        if (handle) {
            glDeleteTextures(1, &handle);
        }
        handle = o.handle;
        width = o.width;
        height = o.height;
        internalFormat = o.internalFormat;
        o.handle = 0;
    }
    return *this;
}

void Texture::bindForAccumulation() const {
    glBindImageTexture(0, handle, 0, GL_FALSE, 0, GL_READ_WRITE, internalFormat);
}

void Texture::bind(int unit, GLenum access) const {
    glBindImageTexture(unit, handle, 0, GL_FALSE, 0, access, internalFormat);
}
