#pragma once

#include <glad/glad.h>

#include <string_view>

namespace GL {

/// Memory-barrier bits, named for what they order rather than for the GL enum.
enum class Barrier : GLbitfield
{
    Storage = GL_SHADER_STORAGE_BARRIER_BIT,
    ImageAccess = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,
    TextureFetch = GL_TEXTURE_FETCH_BARRIER_BIT,
    Framebuffer = GL_FRAMEBUFFER_BARRIER_BIT,
    Command = GL_COMMAND_BARRIER_BIT,
    BufferUpdate = GL_BUFFER_UPDATE_BARRIER_BIT,
};

[[nodiscard]] constexpr Barrier operator|(Barrier a, Barrier b) {
    return static_cast<Barrier>(static_cast<GLbitfield>(a) | static_cast<GLbitfield>(b));
}

inline void memoryBarrier(Barrier barriers) {
    glMemoryBarrier(static_cast<GLbitfield>(barriers));
}

inline void dispatch(GLuint groupsX, GLuint groupsY = 1, GLuint groupsZ = 1) {
    glDispatchCompute(groupsX, groupsY, groupsZ);
}

inline void dispatchIndirect(GLintptr byteOffset) {
    glDispatchComputeIndirect(byteOffset);
}

/// Applied as a whole so no pass inherits another's raster state.
struct RasterState
{
    bool   depthTest = false;
    GLenum depthFunc = GL_LESS;
    bool   depthWrite = true;
    bool   blend = false;
    bool   cullFace = false;
};

inline void applyRasterState(const RasterState& state) {
    state.depthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    glDepthFunc(state.depthFunc);
    glDepthMask(state.depthWrite ? GL_TRUE : GL_FALSE);
    state.blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    state.cullFace ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
}

inline void setViewport(GLsizei width, GLsizei height) {
    glViewport(0, 0, width, height);
}

/// Reversed-Z depends on it; see makeReversedZProjection() in camera.cpp.
inline void setClipDepthZeroToOne() {
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
}

struct ContextInfo
{
    std::string_view vendor;
    std::string_view renderer;
    std::string_view version;
};

[[nodiscard]] inline ContextInfo contextInfo() {
    auto str = [](GLenum name) -> std::string_view {
        const auto* s = reinterpret_cast<const char*>(glGetString(name));
        return s ? std::string_view(s) : std::string_view("?");
    };
    return {str(GL_VENDOR), str(GL_RENDERER), str(GL_VERSION)};
}

} // namespace GL
