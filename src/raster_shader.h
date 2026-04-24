#pragma once

#include <filesystem>

#include "shader_program.h"

class RasterShader : public ShaderProgram
{
public:
    RasterShader() = default;

    RasterShader(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath) {
        trackSource(vertPath);
        trackSource(fragPath);
        ID = buildProgram();
    }

protected:
    GLuint buildProgram() const override {
        if (m_sources.size() < 2) {
            return 0;
        }
        GLuint vs = compileStage(GL_VERTEX_SHADER, m_sources[0].path);
        if (!vs) {
            return 0;
        }
        GLuint fs = compileStage(GL_FRAGMENT_SHADER, m_sources[1].path);
        if (!fs) {
            glDeleteShader(vs);
            return 0;
        }
        return linkProgram({vs, fs});
    }
};
