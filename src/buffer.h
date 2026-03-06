#pragma once

#include <glad/glad.h>
#include <vector>

class Buffer
{
public:
    GLuint id = 0;
    GLenum target;

    Buffer() = default;

    Buffer(GLenum target, GLuint bindingPoint, const void* data, size_t byteSize, GLenum usage)
        : target(target) {
        glCreateBuffers(1, &id);
        glNamedBufferData(id, byteSize, data, usage);
        glBindBufferBase(target, bindingPoint, id);
    }

    // Convenience constructor for vectors
    template<typename T>
    Buffer(GLenum target, GLuint bindingPoint, const std::vector<T>& data, GLenum usage)
        : Buffer(target, bindingPoint, data.data(), data.size() * sizeof(T), usage) {}

    // Convenience constructor for single structs
    template<typename T>
    Buffer(GLenum target, GLuint bindingPoint, const T& data, GLenum usage)
        : Buffer(target, bindingPoint, &data, sizeof(T), usage) {}

    void update(const void* data, size_t byteSize, size_t offset = 0) { glNamedBufferSubData(id, offset, byteSize, data); }

    template<typename T> void update(const std::vector<T>& data, size_t offset = 0) { update(data.data(), data.size() * sizeof(T), offset); }

    template<typename T> void update(const T& data, size_t offset = 0) { update(&data, sizeof(T), offset); }

    void bind() const { glBindBuffer(target, id); }
    void unbind() const { glBindBuffer(target, 0); }

    ~Buffer() {
        if (id)
            glDeleteBuffers(1, &id);
    }

    // Non-copyable, movable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& o) noexcept
        : id(o.id)
        , target(o.target) {
        o.id = 0;
    }
    Buffer& operator=(Buffer&& o) noexcept {
        if (this != &o) {
            if (id)
                glDeleteBuffers(1, &id);
            id = o.id;
            target = o.target;
            o.id = 0;
        }
        return *this;
    }
};