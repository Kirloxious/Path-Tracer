#pragma once

#include <glad/glad.h>

#include "log.h"

// Routes KHR_debug driver messages through the Log:: facility.
// Call enableGLDebug() once after GLAD is loaded. Requires a GL 4.3+ context;
// will silently fall back to no-op if the extension is unavailable.
namespace GLDebug {

inline const char* sourceStr(GLenum source) {
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        return "WINDOW";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        return "SHADER";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        return "THIRD_PARTY";
    case GL_DEBUG_SOURCE_APPLICATION:
        return "APP";
    case GL_DEBUG_SOURCE_OTHER:
        return "OTHER";
    default:
        return "?";
    }
}

inline const char* typeStr(GLenum type) {
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        return "ERROR";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        return "DEPRECATED";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        return "UNDEFINED";
    case GL_DEBUG_TYPE_PORTABILITY:
        return "PORTABILITY";
    case GL_DEBUG_TYPE_PERFORMANCE:
        return "PERF";
    case GL_DEBUG_TYPE_MARKER:
        return "MARKER";
    case GL_DEBUG_TYPE_OTHER:
        return "OTHER";
    default:
        return "?";
    }
}

inline void GLAPIENTRY messageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /*length*/, const GLchar* message,
                                       const void* /*userParam*/) {
    // NVIDIA emits "Buffer detailed info" notifications every time a buffer is allocated;
    // suppressing these keeps the log readable without hiding real issues.
    if (id == 131185) {
        return;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        Log::error("GL[{}/{}] {}: {}", sourceStr(source), typeStr(type), id, message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
    case GL_DEBUG_SEVERITY_LOW:
        Log::warn("GL[{}/{}] {}: {}", sourceStr(source), typeStr(type), id, message);
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    default:
        Log::info("GL[{}/{}] {}: {}", sourceStr(source), typeStr(type), id, message);
        break;
    }
}

inline void enable() {
    GLint flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (!(flags & GL_CONTEXT_FLAG_DEBUG_BIT)) {
        Log::warn("GL debug context not available — request GLFW_OPENGL_DEBUG_CONTEXT to enable driver diagnostics");
        return;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(messageCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    // Silence notification-level spam by default; flip to GL_TRUE if you need it.
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
    Log::info("GL debug callback enabled");
}

} // namespace GLDebug
