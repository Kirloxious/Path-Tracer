#pragma once

/**
 * @file raster_shader.h
 * @brief Vertex + fragment specialization of ShaderProgram.
 */

#include <filesystem>

#include "gpu/shader_program.h"

/// A vertex + fragment program, hot-reloadable via ShaderProgram::reloadIfChanged().
class RasterShader : public ShaderProgram
{
public:
    RasterShader() = default;

    RasterShader(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath)
        : ShaderProgram({{GL_VERTEX_SHADER, vertPath}, {GL_FRAGMENT_SHADER, fragPath}}) {}
};
