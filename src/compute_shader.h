#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "log.h"

class ComputeShader
{
public:
    GLuint ID = 0;

    ComputeShader() = default;

    explicit ComputeShader(const std::filesystem::path& path)
        : ID(loadShader(path))
        , m_path(path) {
        std::error_code ec;
        m_lastWriteTime = std::filesystem::last_write_time(path, ec);
    }

    void use() const { glUseProgram(ID); }

    // Checks whether the shader file has been modified since the last (re)load.
    // If so, recompiles and links a new program. On success, replaces the old program
    // and returns true so the caller can re-upload uniforms and reset accumulation.
    // On compile/link failure, keeps the old program running and returns false.
    bool reloadIfChanged() {
        if (m_path.empty())
            return false;

        std::error_code                 ec;
        std::filesystem::file_time_type currentTime = std::filesystem::last_write_time(m_path, ec);
        if (ec || currentTime == m_lastWriteTime)
            return false;

        // Always advance the stored time so a broken save doesn't spam errors every frame.
        // The user triggers another attempt by saving the file again.
        m_lastWriteTime = currentTime;

        Log::info("Reloading shader: {}", m_path.string());
        GLuint newProgram = loadShader(m_path);
        if (newProgram == 0)
            return false;

        glDeleteProgram(ID);
        ID = newProgram;
        Log::info("Shader reloaded successfully");
        return true;
    }

    void setBool(const std::string& name, bool value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), static_cast<int>(value)); }

    void setInt(const std::string& name, int value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), value); }

    void setFloat(const std::string& name, float value) const { glUniform1f(glGetUniformLocation(ID, name.c_str()), value); }

    void setVec2(const std::string& name, const glm::vec2& value) const { glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }

    void setVec3(const std::string& name, const glm::vec3& value) const { glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }

private:
    std::filesystem::path           m_path;
    std::filesystem::file_time_type m_lastWriteTime{};

    static GLuint loadShader(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            Log::error("Failed to open shader: {}", path.string());
            return 0;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        const std::string shaderSource = ss.str();
        const GLchar*     source = shaderSource.c_str();

        GLuint shaderHandle = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shaderHandle, 1, &source, nullptr);
        glCompileShader(shaderHandle);

        GLint isCompiled = 0;
        glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE) {
            GLint maxLength = 0;
            glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(static_cast<std::size_t>(maxLength));
            glGetShaderInfoLog(shaderHandle, maxLength, &maxLength, infoLog.data());
            Log::error("Shader compile error:\n{}", infoLog.data());
            glDeleteShader(shaderHandle);
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderHandle);
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
            glDeleteShader(shaderHandle);
            return 0;
        }

        glDetachShader(program, shaderHandle);
        glDeleteShader(shaderHandle);
        return program;
    }
};