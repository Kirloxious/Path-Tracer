#pragma once

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

/// Compile/link failures are logged, not thrown: a failed first build leaves id() at 0 and a
/// failed reload keeps the previous program running.
class ShaderProgram
{
public:
    /// Must precede any set*() call.
    void use() const;

    /// A name missing from the linked program is diagnosed once, then ignored.
    void setBool(std::string_view name, bool v) const;
    void setInt(std::string_view name, int v) const;
    void setUInt(std::string_view name, unsigned int v) const;
    void setFloat(std::string_view name, float v) const;
    void setVec2(std::string_view name, const glm::vec2& v) const;
    void setIVec2(std::string_view name, int x, int y) const;
    void setVec3(std::string_view name, const glm::vec3& v) const;
    void setMat4(std::string_view name, const glm::mat4& m) const;

    /// Rebuilds if any entry point or included header changed. Returns true only on a successful rebuild.
    bool reloadIfChanged();

    [[nodiscard]] GLuint id() const { return m_program.get(); }

protected:
    struct Stage
    {
        GLenum                type;
        std::filesystem::path path;
    };

    ShaderProgram() = default;

    /// Paths resolve against the working directory, so the binary must run from the project root.
    explicit ShaderProgram(std::vector<Stage> stages);

private:
    struct WatchedFile
    {
        std::filesystem::path           path;
        std::string                     canonical;
        std::filesystem::file_time_type writeTime;
    };

    /// Transparent hash so string_view lookups don't allocate per set*() call.
    struct StringHash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };

    std::expected<ProgramHandle, std::string>        build();
    std::expected<ShaderHandle, std::string>         compileStage(const Stage& stage);
    static std::expected<ProgramHandle, std::string> link(std::span<const ShaderHandle> stages);

    /// @param seen Canonical paths already included; doubles as the dependency list.
    static std::string preprocessIncludes(const std::filesystem::path& path, std::unordered_set<std::string>& seen);

    /// Headers are never unwatched: pruning on a failed compile would stop watching the very
    /// file being fixed.
    void watch(const std::filesystem::path& path);

    [[nodiscard]] std::string label() const;

    [[nodiscard]] GLint getLocation(std::string_view name) const;

    std::vector<Stage>       m_stages;
    std::vector<WatchedFile> m_watched;
    ProgramHandle            m_program;

    mutable std::unordered_map<std::string, GLint, StringHash, std::equal_to<>> m_locationCache;
};
