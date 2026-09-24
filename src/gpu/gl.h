#pragma once

/**
 * @file gl.h
 * @brief Thin wrappers for the OpenGL commands and state that belong to no single object:
 *        compute dispatch, memory barriers, raster state and context queries.
 *
 * Together with the object wrappers in gpu/ (Buffer, Texture, FrameBuffer, VertexArray,
 * ShaderProgram, the timers), these are the only places outside src/gpu/ that reach OpenGL.
 */

#include <glad/glad.h>

#include <string_view>

namespace GL {

/// Memory-barrier bits, named for what they order rather than for the GL enum.
enum class Barrier : GLbitfield
{
    /// SSBO writes → later SSBO reads.
    Storage = GL_SHADER_STORAGE_BARRIER_BIT,
    /// imageStore → later imageLoad / imageStore.
    ImageAccess = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,
    /// imageStore → later sampler reads of the same texture.
    TextureFetch = GL_TEXTURE_FETCH_BARRIER_BIT,
    /// imageStore → later framebuffer access to the same texture (blits, draws).
    Framebuffer = GL_FRAMEBUFFER_BARRIER_BIT,
    /// SSBO writes → glDispatchComputeIndirect reading them as arguments.
    Command = GL_COMMAND_BARRIER_BIT,
    /// Shader writes → later client-side buffer updates or clears.
    BufferUpdate = GL_BUFFER_UPDATE_BARRIER_BIT,
};

[[nodiscard]] constexpr Barrier operator|(Barrier a, Barrier b) {
    return static_cast<Barrier>(static_cast<GLbitfield>(a) | static_cast<GLbitfield>(b));
}

inline void memoryBarrier(Barrier barriers) {
    glMemoryBarrier(static_cast<GLbitfield>(barriers));
}

/// Launches the currently bound compute program over a grid of work groups.
inline void dispatch(GLuint groupsX, GLuint groupsY = 1, GLuint groupsZ = 1) {
    glDispatchCompute(groupsX, groupsY, groupsZ);
}

/// Launches the current compute program with group counts read from the bound
/// GL_DISPATCH_INDIRECT_BUFFER at @p byteOffset.
inline void dispatchIndirect(GLintptr byteOffset) {
    glDispatchComputeIndirect(byteOffset);
}

/// Fixed-function state for a raster draw, applied as a whole so no pass inherits another's.
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

/// Maps clip-space depth to [0, 1] instead of [-1, 1]. Reversed-Z depends on it; see
/// makeReversedZProjection() in camera.cpp.
inline void setClipDepthZeroToOne() {
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
}

/// Identification strings of the current context. Valid while the context lives.
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
