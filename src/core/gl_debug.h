#pragma once

/**
 * @file gl_debug.h
 * @brief KHR_debug driver-message routing into the Log:: facility.
 */

#include <glad/glad.h>

#include "core/log.h"

/**
 * @brief Routes KHR_debug driver messages through Log::info / warn / error.
 *
 * Call GLDebug::enable() once after GLAD is loaded. Requires a debug GL context
 * (GLFW_OPENGL_DEBUG_CONTEXT, which Window requests in non-NDEBUG builds); without one
 * enable() logs a warning and leaves debug output off.
 */
namespace GLDebug {

/**
 * @brief Maps a `GL_DEBUG_SOURCE_*` enum to a short display string.
 * @param source The GL debug source enum.
 * @return A static string such as "API" or "SHADER"; "?" for unrecognised values.
 */
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

/**
 * @brief Maps a `GL_DEBUG_TYPE_*` enum to a short display string.
 * @param type The GL debug type enum.
 * @return A static string such as "ERROR" or "PERF"; "?" for unrecognised values.
 */
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

/**
 * @brief `glDebugMessageCallback` sink; forwards to the Log:: level matching @p severity.
 *
 * HIGH maps to Log::error, MEDIUM/LOW to Log::warn, NOTIFICATION and anything else to
 * Log::info. NVIDIA's per-allocation "Buffer detailed info" notification (id 131185) is
 * dropped so the log stays readable.
 *
 * @param source   `GL_DEBUG_SOURCE_*` origin of the message.
 * @param type     `GL_DEBUG_TYPE_*` classification.
 * @param id       Driver-assigned message id.
 * @param severity `GL_DEBUG_SEVERITY_*` level, used to pick the log channel.
 * @param message  Null-terminated driver text.
 */
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

/**
 * @brief Installs messageCallback() and enables synchronous debug output.
 *
 * Call once after GLAD has loaded the GL entry points. If the current context was not
 * created with the debug bit, logs a warning and returns without enabling anything.
 * Notification-severity messages are filtered out by default.
 */
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
