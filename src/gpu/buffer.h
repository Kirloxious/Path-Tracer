#pragma once

/**
 * @file buffer.h
 * @brief RAII OpenGL buffer object built on direct state access.
 */

#include <glad/glad.h>
#include <type_traits>
#include <vector>

#include "core/log.h"
#include "gpu/gl_handle.h"

/**
 * @brief Owns one GL buffer (SSBO / UBO / dispatch-indirect).
 *
 * Construction only allocates. Binding is a separate, explicit bindBase() call made by
 * whoever dispatches the work that reads it, so a buffer's binding point is visible at the
 * dispatch site instead of being a side effect of when it happened to be created.
 *
 * Move-only; a moved-from Buffer has `id() == 0`.
 */
class Buffer
{
public:
    Buffer() = default;

    /**
     * @brief Allocates a buffer from a raw pointer + byte count.
     *
     * `byteSize == 0` warns: an empty SSBO/UBO is legal but almost always an empty container
     * upstream, which otherwise shows up only as a black image.
     *
     * @param data     Source bytes, or nullptr to allocate uninitialised storage.
     * @param byteSize Allocation size in bytes.
     * @param usage    GL usage hint (GL_STATIC_DRAW, GL_DYNAMIC_COPY, ...).
     */
    Buffer(const void* data, size_t byteSize, GLenum usage) {
        if (byteSize == 0) {
            Log::warn("Buffer allocated with byteSize=0");
        }
        GLuint id = 0;
        glCreateBuffers(1, &id);
        m_handle.reset(id);
        glNamedBufferData(id, byteSize, data, usage);
    }

    /**
     * @brief Uploads a whole `std::vector`.
     * @tparam T     Element type; must match the shader's std430/std140 layout.
     * @param data   Elements to upload; `data.size() * sizeof(T)` bytes are allocated.
     * @param usage  GL usage hint.
     */
    template<typename T>
        requires std::is_trivially_copyable_v<T>
    Buffer(const std::vector<T>& data, GLenum usage) : Buffer(data.data(), data.size() * sizeof(T), usage) {}

    /**
     * @brief Uploads a single struct (e.g. a UBO).
     *
     * Pointers are excluded so `Buffer(&x, usage)` cannot bind T to a pointer type and
     * upload sizeof(pointer) bytes of address instead of the object.
     *
     * @tparam T     Struct type; must match the shader's std430/std140 layout.
     * @param data   The object to upload; pass the object, not its address.
     * @param usage  GL usage hint.
     */
    template<typename T>
        requires(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
    Buffer(const T& data, GLenum usage) : Buffer(&data, sizeof(T), usage) {}

    /**
     * @brief Overwrites part of the buffer in place.
     * @param data     Source bytes.
     * @param byteSize Number of bytes to write.
     * @param offset   Destination byte offset into the buffer.
     */
    void update(const void* data, size_t byteSize, size_t offset = 0) { glNamedBufferSubData(id(), offset, byteSize, data); }

    /**
     * @brief Overwrites the buffer with a single struct.
     *
     * Same pointer guard as the single-struct constructor. Without it, `update(&x, sizeof(x))`
     * binds T = decltype(&x) by identity — which beats the raw-pointer overload's
     * pointer-to-void conversion — and silently uploads the pointer's own bits.
     *
     * @tparam T     Struct type.
     * @param data   The object to write.
     * @param offset Destination byte offset into the buffer.
     */
    template<typename T>
        requires(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
    void update(const T& data, size_t offset = 0) {
        update(&data, sizeof(T), offset);
    }

    /// Zeroes every byte.
    void clear() const { glClearNamedBufferData(id(), GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr); }

    /**
     * @brief Binds to an indexed binding point.
     * @param target GL_SHADER_STORAGE_BUFFER or GL_UNIFORM_BUFFER.
     * @param index  Binding index — a BIND_* / UBO_* constant from core/shader_shared.h.
     */
    void bindBase(GLenum target, GLuint index) const { glBindBufferBase(target, index, id()); }

    /// Binds to a non-indexed target such as GL_DISPATCH_INDIRECT_BUFFER.
    void bind(GLenum target) const { glBindBuffer(target, id()); }

    [[nodiscard]] GLuint id() const { return m_handle.get(); }

private:
    BufferHandle m_handle;
};
