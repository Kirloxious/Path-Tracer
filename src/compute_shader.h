#pragma once

#include <filesystem>

#include "shader_program.h"

class ComputeShader : public ShaderProgram
{
public:
    ComputeShader() = default;
    explicit ComputeShader(const std::filesystem::path& path);

protected:
    GLuint buildProgram() const override;
};
