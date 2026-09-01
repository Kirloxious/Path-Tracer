#pragma once

/**
 * @file shader_program.h
 * @brief Common base for every GPU program: ownership, uniform setters and hot reload.
 */

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Base for every GPU program in this project.
 *
 * Owns the `GLuint ID`, tracks the set of source files so hot-reload is generic across
 * pipeline types, and exposes shared uniform setters plus uniform-location diagnostics.
 * Subclasses implement buildProgram() to compile and link their own stages.
 *
 * Nothing here throws: compile and link failures are reported through Log::error and leave
 * the previously linked program (or 0) in place.
 *
 * Non-copyable, movable.
 */
class ShaderProgram
{
public:
    GLuint ID = 0;

    ShaderProgram() = default;
    virtual ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&& o) noexcept;
    ShaderProgram& operator=(ShaderProgram&& o) noexcept;

    /// Makes this program current (`glUseProgram`). Must precede any set*() call.
    void use() const;

    /// @name Uniform setters
    /// Each looks @p name up through the location cache and writes @p v into the currently
    /// bound program. A name that does not exist in the linked program is diagnosed once and
    /// then silently ignored, so an unused uniform is not fatal.
    /// @{

    /** @param name Uniform name in the shader. @param v Value to write. */
    void setBool(const std::string& name, bool v) const;
    /** @param name Uniform name in the shader. @param v Value to write. */
    void setInt(const std::string& name, int v) const;
    /** @param name Uniform name in the shader. @param v Value to write. */
    void setFloat(const std::string& name, float v) const;
    /** @param name Uniform name in the shader. @param v Value to write. */
    void setVec2(const std::string& name, const glm::vec2& v) const;
    /** @param name Uniform name in the shader. @param x First component. @param y Second component. */
    void setIVec2(const std::string& name, int x, int y) const;
    /** @param name Uniform name in the shader. @param v Value to write. */
    void setVec3(const std::string& name, const glm::vec3& v) const;
    /** @param name Uniform name in the shader. @param m Matrix to write (column-major, no transpose). */
    void setMat4(const std::string& name, const glm::mat4& m) const;

    /// @}

    /**
     * @brief Rebuilds the program if any tracked source file changed on disk.
     *
     * Polls file timestamps for every tracked source — entry points *and* the headers they
     * `#include`. Called once per frame by the owning pass.
     *
     * @return true if a rebuild succeeded and `ID` now refers to a new program; false if
     *         nothing changed, or if the rebuild failed and the old program is still running.
     */
    bool reloadIfChanged();

protected:
    /// A watched file plus the mtime it had at the last successful build.
    struct Source
    {
        std::filesystem::path           path;
        std::filesystem::file_time_type writeTime;
    };

    // Entry-point stages, registered by the subclass constructor.
    std::vector<Source> m_sources;

    // Headers discovered by the preprocessor during the last build. Watched alongside
    // m_sources so editing shader/common/*.glsl actually triggers a reload. Never pruned:
    // a header dropped from an #include just costs one stat() per frame, whereas pruning
    // on a failed compile would stop watching the file you are mid-edit on.
    std::vector<Source> m_includes;

    // Cache of name → location. glGetUniformLocation is a driver-side string lookup;
    // called per setInt/setFloat across 8+ shaders this adds up. Cleared on reload —
    // a recompiled program may have relocated or eliminated uniforms.
    mutable std::unordered_map<std::string, GLint> m_locationCache;

    /**
     * @brief Compiles and links this program's stages.
     *
     * Non-const because compileStage() records the include graph it walks into `m_includes`.
     *
     * @return A new program handle, or 0 on failure.
     */
    virtual GLuint buildProgram() = 0;

    /**
     * @brief Records a file to watch for hot-reload. Called by subclass constructors.
     * @param path Entry-point source file to stat each frame.
     */
    void trackSource(const std::filesystem::path& path);

    /// @return A comma-joined list of tracked entry-point filenames, for log messages.
    std::string sourcesLabel() const;

    /**
     * @brief Looks a uniform location up, consulting and populating the location cache.
     * @param name Uniform name in the linked program.
     * @return The uniform location, or -1 if the program has no such active uniform.
     */
    GLint getLocation(const std::string& name) const;

    /**
     * @brief Recursively expands `#include` directives into a single translation unit.
     * @param path Source file to read.
     * @param seen In/out set of already-included canonical paths; prevents double inclusion
     *             and doubles as the include graph recordIncludes() consumes.
     * @return The fully expanded GLSL source, or an empty string if @p path could not be read.
     */
    static std::string preprocessIncludes(const std::filesystem::path& path, std::unordered_set<std::string>& seen);

    /**
     * @brief Preprocesses and compiles one shader stage.
     * @param stage GL stage enum (GL_COMPUTE_SHADER, GL_VERTEX_SHADER, ...).
     * @param path  Source file for the stage.
     * @return The compiled shader object, or 0 on failure (the compile log is written to Log::error).
     */
    GLuint compileStage(GLenum stage, const std::filesystem::path& path);

    /**
     * @brief Links compiled stages into a program and detaches them.
     * @param stages Shader objects produced by compileStage().
     * @return The linked program handle, or 0 on failure.
     */
    static GLuint linkProgram(const std::vector<GLuint>& stages);

    /**
     * @brief Adds any path in @p seen that isn't already an entry point or a known include.
     * @param seen Canonical include paths visited during the last preprocess.
     */
    void recordIncludes(const std::unordered_set<std::string>& seen);
};
