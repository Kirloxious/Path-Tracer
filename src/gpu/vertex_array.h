#pragma once

#include <glad/glad.h>

#include "gpu/buffer.h"
#include "gpu/gl_handle.h"

/// Borrows the Buffers it is pointed at; they must outlive it.
class VertexArray
{
public:
    VertexArray() = default;

    [[nodiscard]] static VertexArray create() {
        GLuint id = 0;
        glCreateVertexArrays(1, &id);
        VertexArray vao;
        vao.m_handle.reset(id);
        return vao;
    }

    void setVertexBuffer(GLuint binding, const Buffer& buffer, GLsizei stride, GLintptr offset = 0) {
        glVertexArrayVertexBuffer(id(), binding, buffer.id(), offset, stride);
    }

    void setElementBuffer(const Buffer& buffer) { glVertexArrayElementBuffer(id(), buffer.id()); }

    void setFloatAttribute(GLuint index, GLuint binding, GLint components, GLuint offset) {
        glEnableVertexArrayAttrib(id(), index);
        glVertexArrayAttribFormat(id(), index, components, GL_FLOAT, GL_FALSE, offset);
        glVertexArrayAttribBinding(id(), index, binding);
    }

    void setUIntAttribute(GLuint index, GLuint binding, GLint components, GLuint offset) {
        glEnableVertexArrayAttrib(id(), index);
        glVertexArrayAttribIFormat(id(), index, components, GL_UNSIGNED_INT, offset);
        glVertexArrayAttribBinding(id(), index, binding);
    }

    void drawTriangles(GLsizei indexCount) const {
        glBindVertexArray(id());
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    [[nodiscard]] GLuint id() const { return m_handle.get(); }

private:
    VertexArrayHandle m_handle;
};
