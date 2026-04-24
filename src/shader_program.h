#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "log.h"

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
    virtual ~ShaderProgram() {
        if (ID) {
            glDeleteProgram(ID);
        }
    }

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&& o) noexcept : ID(o.ID), m_sources(std::move(o.m_sources)) { o.ID = 0; }
    ShaderProgram& operator=(ShaderProgram&& o) noexcept {
        if (this != &o) {
            if (ID) {
                glDeleteProgram(ID);
            }
            ID = o.ID;
            m_sources = std::move(o.m_sources);
            o.ID = 0;
        }
        return *this;
    }

    void use() const { glUseProgram(ID); }

    void setBool(const std::string& name, bool v) const { glUniform1i(getLocation(name), static_cast<int>(v)); }
    void setInt(const std::string& name, int v) const { glUniform1i(getLocation(name), v); }
    void setFloat(const std::string& name, float v) const { glUniform1f(getLocation(name), v); }
    void setVec2(const std::string& name, const glm::vec2& v) const { glUniform2fv(getLocation(name), 1, &v[0]); }
    void setVec3(const std::string& name, const glm::vec3& v) const { glUniform3fv(getLocation(name), 1, &v[0]); }
    void setMat4(const std::string& name, const glm::mat4& m) const { glUniformMatrix4fv(getLocation(name), 1, GL_FALSE, &m[0][0]); }

    // Polls file timestamps for every tracked source; rebuilds on change.
    // On failure keeps the old program running and returns false.
    bool reloadIfChanged() {
        if (m_sources.empty()) {
            return false;
        }

        bool anyChanged = false;
        for (auto& src : m_sources) {
            std::error_code ec;
            auto            t = std::filesystem::last_write_time(src.path, ec);
            if (!ec && t != src.writeTime) {
                src.writeTime = t; // advance even on failure so we don't spam errors
                anyChanged = true;
            }
        }
        if (!anyChanged) {
            return false;
        }

        Log::info("Reloading shader: {}", sourcesLabel());
        GLuint newProgram = buildProgram();
        if (newProgram == 0) {
            return false;
        }

        if (ID) {
            glDeleteProgram(ID);
        }
        ID = newProgram;
        Log::info("Shader reloaded successfully");
        return true;
    }

protected:
    struct Source
    {
        std::filesystem::path           path;
        std::filesystem::file_time_type writeTime;
    };

    std::vector<Source> m_sources;

    // Compile + link stages; return a new program handle or 0 on failure.
    virtual GLuint buildProgram() const = 0;

    // Record a file to watch for hot-reload. Called by subclass constructors.
    void trackSource(const std::filesystem::path& path) {
        std::error_code ec;
        m_sources.push_back({path, std::filesystem::last_write_time(path, ec)});
    }

    std::string sourcesLabel() const {
        std::string out;
        for (size_t i = 0; i < m_sources.size(); ++i) {
            if (i) {
                out += " + ";
            }
            out += m_sources[i].path.filename().string();
        }
        return out;
    }

    GLint getLocation(const std::string& name) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc == -1) {
            static thread_local std::unordered_set<std::string> warned;
            if (warned.insert(sourcesLabel() + ":" + name).second) {
                Log::warn("Uniform '{}' not found in {} (unused or typo)", name, sourcesLabel());
            }
        }
        return loc;
    }

    // Shared compile/link helpers used by every subclass.

    static std::string readFile(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            Log::error("Failed to open shader: {}", path.string());
            return {};
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    static GLuint compileStage(GLenum stage, const std::filesystem::path& path) {
        std::string source = readFile(path);
        if (source.empty()) {
            return 0;
        }

        const GLchar* src = source.c_str();
        GLuint        shader = glCreateShader(stage);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint isCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE) {
            GLint maxLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(static_cast<std::size_t>(maxLength));
            glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());
            Log::error("Shader compile error ({}):\n{}", path.string(), infoLog.data());
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static GLuint linkProgram(const std::vector<GLuint>& stages) {
        GLuint program = glCreateProgram();
        for (GLuint s : stages) {
            glAttachShader(program, s);
        }
        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE) {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(static_cast<std::size_t>(maxLength));
            glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
            Log::error("Program link error:\n{}", infoLog.data());
            glDeleteProgram(program);
            for (GLuint s : stages) {
                glDeleteShader(s);
            }
            return 0;
        }

        for (GLuint s : stages) {
            glDetachShader(program, s);
            glDeleteShader(s);
        }
        return program;
    }
};
