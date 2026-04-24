#pragma once

#include <filesystem>

#include "shader_program.h"

class ComputeShader : public ShaderProgram
{
public:
    ComputeShader() = default;

    explicit ComputeShader(const std::filesystem::path& path) {
        trackSource(path);
        ID = buildProgram();
    }

protected:
    GLuint buildProgram() const override {
        if (m_sources.empty()) {
            return 0;
        }
        GLuint cs = compileStage(GL_COMPUTE_SHADER, m_sources[0].path);
        if (!cs) {
            return 0;
        }
        return linkProgram({cs});
    }
};
