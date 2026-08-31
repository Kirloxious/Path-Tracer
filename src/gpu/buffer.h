#pragma once

#include <glad/glad.h>
#include <type_traits>
#include <vector>

#include "core/log.h"

class Buffer
{
public:
    GLuint id = 0;
    GLenum target = 0;

    Buffer() = default;

    Buffer(GLenum target, GLuint bindingPoint, const void* data, size_t byteSize, GLenum usage) : target(target) {
        // A zero-sized SSBO/UBO is legal but almost always a bug: the GPU shader will read
        // garbage or the driver will silently bind a dummy buffer. Warn loudly so the empty
        // container upstream gets caught immediately instead of producing a black image.
        if (byteSize == 0) {
            Log::warn("Buffer::Buffer called with byteSize=0 (target=0x{:x}, binding={})", target, bindingPoint);
        }
        glCreateBuffers(1, &id);
        glNamedBufferData(id, byteSize, data, usage);
        glBindBufferBase(target, bindingPoint, id);
    }

    // Convenience constructor for vectors
    template<typename T>
    Buffer(GLenum target, GLuint bindingPoint, const std::vector<T>& data, GLenum usage)
        : Buffer(target, bindingPoint, data.data(), data.size() * sizeof(T), usage) {}

    // Convenience constructor for single structs. Constrained away from pointers so a
    // `Buffer(target, binding, ptr, byteSize, usage)` call can never bind T = some pointer
    // type and upload sizeof(pointer) bytes instead of the buffer it points at.
    template<typename T>
        requires(!std::is_pointer_v<T>)
    Buffer(GLenum target, GLuint bindingPoint, const T& data, GLenum usage) : Buffer(target, bindingPoint, &data, sizeof(T), usage) {}

    void update(const void* data, size_t byteSize, size_t offset = 0) { glNamedBufferSubData(id, offset, byteSize, data); }

    template<typename T> void update(const std::vector<T>& data, size_t offset = 0) { update(data.data(), data.size() * sizeof(T), offset); }

    // Same guard as the single-struct constructor. Without it, `update(&x, sizeof(x))`
    // binds T = decltype(&x) by identity — which beats the raw-pointer overload's
    // pointer-to-void conversion — and silently uploads the pointer's own bits at
    // `offset = sizeof(x)`. Pass the object, not its address.
    template<typename T>
        requires(!std::is_pointer_v<T>)
    void update(const T& data, size_t offset = 0) {
        update(&data, sizeof(T), offset);
    }

    void bind() const { glBindBuffer(target, id); }
    void unbind() const { glBindBuffer(target, 0); }

    ~Buffer() {
        if (id) {
            glDeleteBuffers(1, &id);
        }
    }

    // Non-copyable, movable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& o) noexcept : id(o.id), target(o.target) { o.id = 0; }
    Buffer& operator=(Buffer&& o) noexcept {
        if (this != &o) {
            if (id) {
                glDeleteBuffers(1, &id);
            }
            id = o.id;
            target = o.target;
            o.id = 0;
        }
        return *this;
    }
};