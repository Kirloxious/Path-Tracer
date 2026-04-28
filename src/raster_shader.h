#pragma once

#include <filesystem>

#include "shader_program.h"

class RasterShader : public ShaderProgram
{
public:
    RasterShader() = default;
    RasterShader(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath);

protected:
    GLuint buildProgram() const override;
};
