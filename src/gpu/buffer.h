#pragma once

/**
 * @file buffer.h
 * @brief RAII OpenGL buffer object built on direct state access.
 */

#include <glad/glad.h>
#include <type_traits>
#include <vector>

#include "core/log.h"

/**
 * @brief Owns one GL buffer (SSBO / UBO / dispatch-indirect) and binds it to an index.
 *
 * Every constructor allocates with `glNamedBufferData` and immediately calls
 * `glBindBufferBase(target, bindingPoint, id)`, so a Buffer is live at its binding for the
 * rest of its lifetime — the binding map in CLAUDE.md is the contract with the shaders.
 *
 * Non-copyable, movable — a moved-from Buffer has `id == 0` and deletes nothing.
 */
class Buffer
{
public:
    GLuint id = 0;
    GLenum target = 0;

    Buffer() = default;

    /**
     * @brief Allocates a buffer from a raw pointer + byte count and binds it.
     *
     * A zero-sized SSBO/UBO is legal but almost always a bug: the shader will read garbage or
     * the driver will silently bind a dummy buffer. `byteSize == 0` therefore warns loudly, so
     * the empty container upstream gets caught immediately instead of producing a black image.
     *
     * @param target       GL_SHADER_STORAGE_BUFFER, GL_UNIFORM_BUFFER, etc.
     * @param bindingPoint Binding index the shaders declare for this buffer.
     * @param data         Source bytes, or nullptr to allocate uninitialised storage.
     * @param byteSize     Allocation size in bytes.
     * @param usage        GL usage hint (GL_STATIC_DRAW, GL_DYNAMIC_COPY, ...).
     */
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

    /**
     * @brief Convenience constructor uploading a whole `std::vector`.
     * @tparam T           Element type; must match the shader's std430/std140 layout.
     * @param target       GL buffer target.
     * @param bindingPoint Binding index the shaders declare for this buffer.
     * @param data         Elements to upload; `data.size() * sizeof(T)` bytes are allocated.
     * @param usage        GL usage hint.
     */
    template<typename T>
    Buffer(GLenum target, GLuint bindingPoint, const std::vector<T>& data, GLenum usage)
        : Buffer(target, bindingPoint, data.data(), data.size() * sizeof(T), usage) {}

    /**
     * @brief Convenience constructor uploading a single struct (e.g. the camera UBO).
     *
     * Constrained away from pointers so a `Buffer(target, binding, ptr, byteSize, usage)` call
     * can never bind T = some pointer type and upload sizeof(pointer) bytes instead of the
     * buffer it points at.
     *
     * @tparam T           Struct type; must match the shader's std430/std140 layout.
     * @param target       GL buffer target.
     * @param bindingPoint Binding index the shaders declare for this buffer.
     * @param data         The object to upload; pass the object, not its address.
     * @param usage        GL usage hint.
     */
    template<typename T>
        requires(!std::is_pointer_v<T>)
    Buffer(GLenum target, GLuint bindingPoint, const T& data, GLenum usage) : Buffer(target, bindingPoint, &data, sizeof(T), usage) {}

    /**
     * @brief Overwrites part of the buffer in place.
     * @param data     Source bytes.
     * @param byteSize Number of bytes to write.
     * @param offset   Destination byte offset into the buffer.
     */
    void update(const void* data, size_t byteSize, size_t offset = 0) { glNamedBufferSubData(id, offset, byteSize, data); }

    /**
     * @brief Overwrites the buffer with a whole vector's contents.
     * @tparam T     Element type.
     * @param data   Elements to write.
     * @param offset Destination byte offset into the buffer.
     */
    template<typename T> void update(const std::vector<T>& data, size_t offset = 0) { update(data.data(), data.size() * sizeof(T), offset); }

    /**
     * @brief Overwrites the buffer with a single struct.
     *
     * Same pointer guard as the single-struct constructor. Without it, `update(&x, sizeof(x))`
     * binds T = decltype(&x) by identity — which beats the raw-pointer overload's
     * pointer-to-void conversion — and silently uploads the pointer's own bits at
     * `offset = sizeof(x)`. Pass the object, not its address.
     *
     * @tparam T     Struct type.
     * @param data   The object to write.
     * @param offset Destination byte offset into the buffer.
     */
    template<typename T>
        requires(!std::is_pointer_v<T>)
    void update(const T& data, size_t offset = 0) {
        update(&data, sizeof(T), offset);
    }

    /// Binds to the buffer's target slot (distinct from the indexed binding done at construction).
    void bind() const { glBindBuffer(target, id); }
    /// Unbinds whatever is bound to the buffer's target slot. Does not affect the indexed binding.
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
