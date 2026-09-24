#pragma once

/**
 * @file vertex_array.h
 * @brief RAII vertex array object built on direct state access.
 */

#include <glad/glad.h>

#include "gpu/buffer.h"
#include "gpu/gl_handle.h"

/**
 * @brief Owns one VAO: vertex-buffer bindings, attribute formats and the element buffer.
 *
 * Borrows the Buffers it is pointed at — they must outlive the VertexArray. Move-only.
 */
class VertexArray
{
public:
    /// An empty object; create() allocates the GL name.
    VertexArray() = default;

    [[nodiscard]] static VertexArray create() {
        GLuint id = 0;
        glCreateVertexArrays(1, &id);
        VertexArray vao;
        vao.m_handle.reset(id);
        return vao;
    }

    /// Sources vertex-buffer binding @p binding from @p buffer, one vertex every @p stride bytes.
    void setVertexBuffer(GLuint binding, const Buffer& buffer, GLsizei stride, GLintptr offset = 0) {
        glVertexArrayVertexBuffer(id(), binding, buffer.id(), offset, stride);
    }

    void setElementBuffer(const Buffer& buffer) { glVertexArrayElementBuffer(id(), buffer.id()); }

    /// Enables attribute @p index as @p components floats at @p offset within binding @p binding.
    void setFloatAttribute(GLuint index, GLuint binding, GLint components, GLuint offset) {
        glEnableVertexArrayAttrib(id(), index);
        glVertexArrayAttribFormat(id(), index, components, GL_FLOAT, GL_FALSE, offset);
        glVertexArrayAttribBinding(id(), index, binding);
    }

    /// Enables attribute @p index as @p components unsigned integers, read without conversion.
    void setUIntAttribute(GLuint index, GLuint binding, GLint components, GLuint offset) {
        glEnableVertexArrayAttrib(id(), index);
        glVertexArrayAttribIFormat(id(), index, components, GL_UNSIGNED_INT, offset);
        glVertexArrayAttribBinding(id(), index, binding);
    }

    /// Draws @p indexCount uint32 indices from the element buffer as triangles, with the
    /// currently bound program and framebuffer.
    void drawTriangles(GLsizei indexCount) const {
        glBindVertexArray(id());
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    [[nodiscard]] GLuint id() const { return m_handle.get(); }

private:
    VertexArrayHandle m_handle;
};
