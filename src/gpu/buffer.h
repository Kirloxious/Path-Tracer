#pragma once

#include <glad/glad.h>
#include <span>
#include <type_traits>
#include <vector>

#include "core/log.h"
#include "gpu/gl_handle.h"

/// Construction only allocates; binding is an explicit bindBase() at the dispatch site.
class Buffer
{
public:
    Buffer() = default;

    /// Warns on byteSize == 0: almost always an empty container upstream, which otherwise shows
    /// up only as a black image.
    Buffer(const void* data, size_t byteSize, GLenum usage) {
        if (byteSize == 0) {
            Log::warn("Buffer allocated with byteSize=0");
        }
        GLuint id = 0;
        glCreateBuffers(1, &id);
        m_handle.reset(id);
        glNamedBufferData(id, byteSize, data, usage);
    }

    template<typename T>
        requires std::is_trivially_copyable_v<T>
    Buffer(std::span<const T> data, GLenum usage) : Buffer(data.data(), data.size_bytes(), usage) {}

    template<typename T>
        requires std::is_trivially_copyable_v<T>
    Buffer(const std::vector<T>& data, GLenum usage) : Buffer(std::span<const T>(data), usage) {}

    /// Pointers are excluded so `Buffer(&x, usage)` cannot upload the address instead of the object.
    template<typename T>
        requires(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
    Buffer(const T& data, GLenum usage) : Buffer(&data, sizeof(T), usage) {}

    void update(const void* data, size_t byteSize, size_t offset = 0) { glNamedBufferSubData(id(), offset, byteSize, data); }

    /// Pointer guard as above: without it `update(&x, sizeof(x))` binds T = decltype(&x), beating
    /// the raw-pointer overload, and uploads the pointer's own bits.
    template<typename T>
        requires(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
    void update(const T& data, size_t offset = 0) {
        update(&data, sizeof(T), offset);
    }

    void clear() const { glClearNamedBufferData(id(), GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr); }

    void bindBase(GLenum target, GLuint index) const { glBindBufferBase(target, index, id()); }

    void bind(GLenum target) const { glBindBuffer(target, id()); }

    [[nodiscard]] GLuint id() const { return m_handle.get(); }

private:
    BufferHandle m_handle;
};
