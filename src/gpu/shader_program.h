#pragma once

/**
 * @file shader_program.h
 * @brief Common base for every GPU program: ownership, uniform setters and hot reload.
 */

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <expected>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "gpu/gl_handle.h"

/**
 * @brief A linked GL program built from a fixed list of stage sources, with hot reload.
 *
 * Tracks every entry point and every header it `#include`s, and rebuilds when any of them
 * changes on disk. Compile and link failures are logged, not thrown: on first build they
 * leave id() at 0, and on a reload they keep the previous program running, so a broken edit
 * can be fixed without restarting.
 *
 * Move-only.
 */
class ShaderProgram
{
public:
    /// Makes this program current (`glUseProgram`). Must precede any set*() call.
    void use() const;

    /// @name Uniform setters
    /// Each looks @p name up through the location cache and writes @p v into the currently
    /// bound program. A name that does not exist in the linked program is diagnosed once and
    /// then silently ignored, so an unused uniform is not fatal.
    /// @{
    void setBool(std::string_view name, bool v) const;
    void setInt(std::string_view name, int v) const;
    void setUInt(std::string_view name, unsigned int v) const;
    void setFloat(std::string_view name, float v) const;
    void setVec2(std::string_view name, const glm::vec2& v) const;
    void setIVec2(std::string_view name, int x, int y) const;
    void setVec3(std::string_view name, const glm::vec3& v) const;
    void setMat4(std::string_view name, const glm::mat4& m) const;
    /// @}

    /**
     * @brief Rebuilds the program if any tracked source file changed on disk.
     *
     * Called once per frame by the owning pass.
     *
     * @return true if a rebuild succeeded and id() now refers to a new program; false if
     *         nothing changed, or if the rebuild failed and the old program is still running.
     */
    bool reloadIfChanged();

    [[nodiscard]] GLuint id() const { return m_program.get(); }

protected:
    struct Stage
    {
        GLenum                type;
        std::filesystem::path path;
    };

    ShaderProgram() = default;

    /// Compiles and links @p stages. Paths resolve against the working directory, which is
    /// why the binary must run from the project root.
    explicit ShaderProgram(std::vector<Stage> stages);

private:
    struct WatchedFile
    {
        std::filesystem::path           path;
        std::string                     canonical;
        std::filesystem::file_time_type writeTime;
    };

    /// Transparent hash so string_view lookups don't allocate a std::string per set*() call.
    struct StringHash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };

    std::expected<ProgramHandle, std::string>        build();
    std::expected<ShaderHandle, std::string>         compileStage(const Stage& stage);
    static std::expected<ProgramHandle, std::string> link(std::span<const ShaderHandle> stages);

    /**
     * @brief Recursively expands `#include "..."` directives into a single translation unit.
     * @param path Source file to read.
     * @param seen In/out set of already-included canonical paths; prevents double inclusion
     *             and doubles as the list of files the result depends on.
     * @return The fully expanded GLSL source, or an empty string if @p path could not be read.
     */
    static std::string preprocessIncludes(const std::filesystem::path& path, std::unordered_set<std::string>& seen);

    /// Starts watching @p path unless it already is. Headers are never unwatched: a header
    /// dropped from an `#include` costs one stat() per frame, whereas pruning on a failed
    /// compile would stop watching the very file being fixed.
    void watch(const std::filesystem::path& path);

    /// @return The entry-point filenames joined with " + ", for log messages.
    [[nodiscard]] std::string label() const;

    [[nodiscard]] GLint getLocation(std::string_view name) const;

    std::vector<Stage>       m_stages;
    std::vector<WatchedFile> m_watched;
    ProgramHandle            m_program;

    // Cleared on reload — a recompiled program may have relocated or eliminated uniforms.
    mutable std::unordered_map<std::string, GLint, StringHash, std::equal_to<>> m_locationCache;
};
