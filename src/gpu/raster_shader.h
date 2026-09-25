#pragma once

#include <filesystem>

#include "gpu/shader_program.h"

class RasterShader : public ShaderProgram
{
public:
    RasterShader() = default;

    RasterShader(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath)
        : ShaderProgram({{GL_VERTEX_SHADER, vertPath}, {GL_FRAGMENT_SHADER, fragPath}}) {}
};
