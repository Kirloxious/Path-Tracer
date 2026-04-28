#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

// Base for every GPU program in this project. Owns the `GLuint ID`, tracks the
// set of source files so hot-reload is generic across pipeline types, and
// exposes shared uniform setters + uniform-location diagnostics.
//
// Subclasses implement `buildProgram()` to compile and link their stages.
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

    void use() const;

    void setBool(const std::string& name, bool v) const;
    void setInt(const std::string& name, int v) const;
    void setFloat(const std::string& name, float v) const;
    void setVec2(const std::string& name, const glm::vec2& v) const;
    void setVec3(const std::string& name, const glm::vec3& v) const;
    void setMat4(const std::string& name, const glm::mat4& m) const;

    // Polls file timestamps for every tracked source; rebuilds on change.
    // On failure keeps the old program running and returns false.
    bool reloadIfChanged();

protected:
    struct Source
    {
        std::filesystem::path           path;
        std::filesystem::file_time_type writeTime;
        bool                            isEntryPoint = false;
    };

    std::vector<Source> m_sources;

    // Compile + link stages; return a new program handle or 0 on failure.
    virtual GLuint buildProgram() const = 0;

    // Record a file to watch for hot-reload. Called by subclass constructors.
    void trackSource(const std::filesystem::path& path);

    std::string sourcesLabel() const;

    GLint getLocation(const std::string& name) const;

    // Shared compile/link helpers used by every subclass.
    static std::string preprocessIncludes(const std::filesystem::path& path, std::unordered_set<std::string>& seen);
    static GLuint      compileStage(GLenum stage, const std::filesystem::path& path);
    static GLuint      linkProgram(const std::vector<GLuint>& stages);
};
