#pragma once

#include <glad/glad.h>

#include <utility>

/// @tparam Delete Releases a non-zero name; never called with 0.
template<void (*Delete)(GLuint)> class GLHandle
{
public:
    GLHandle() = default;
    explicit GLHandle(GLuint id) noexcept : m_id(id) {}
    ~GLHandle() { reset(); }

    GLHandle(GLHandle&& o) noexcept : m_id(std::exchange(o.m_id, 0)) {}
    GLHandle& operator=(GLHandle&& o) noexcept {
        if (this != &o) {
            reset(std::exchange(o.m_id, 0));
        }
        return *this;
    }

    GLHandle(const GLHandle&) = delete;
    GLHandle& operator=(const GLHandle&) = delete;

    void reset(GLuint id = 0) noexcept {
        if (m_id) {
            Delete(m_id);
        }
        m_id = id;
    }

    [[nodiscard]] GLuint get() const noexcept { return m_id; }
    explicit             operator bool() const noexcept { return m_id != 0; }

private:
    GLuint m_id = 0;
};

namespace GLDelete {
inline void buffer(GLuint id) {
    glDeleteBuffers(1, &id);
}
inline void texture(GLuint id) {
    glDeleteTextures(1, &id);
}
inline void framebuffer(GLuint id) {
    glDeleteFramebuffers(1, &id);
}
inline void vertexArray(GLuint id) {
    glDeleteVertexArrays(1, &id);
}
inline void query(GLuint id) {
    glDeleteQueries(1, &id);
}
inline void program(GLuint id) {
    glDeleteProgram(id);
}
inline void shader(GLuint id) {
    glDeleteShader(id);
}
} // namespace GLDelete

using BufferHandle = GLHandle<GLDelete::buffer>;
using TextureHandle = GLHandle<GLDelete::texture>;
using FramebufferHandle = GLHandle<GLDelete::framebuffer>;
using VertexArrayHandle = GLHandle<GLDelete::vertexArray>;
using QueryHandle = GLHandle<GLDelete::query>;
using ProgramHandle = GLHandle<GLDelete::program>;
using ShaderHandle = GLHandle<GLDelete::shader>;
