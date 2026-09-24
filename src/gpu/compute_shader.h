#pragma once

/**
 * @file compute_shader.h
 * @brief Compute-stage specialization of ShaderProgram.
 */

#include <filesystem>

#include "gpu/shader_program.h"

/// A single-stage compute program, hot-reloadable via ShaderProgram::reloadIfChanged().
class ComputeShader : public ShaderProgram
{
public:
    ComputeShader() = default;

    /// Loads, preprocesses `#include`s in, compiles and links a `.comp` source file.
    explicit ComputeShader(const std::filesystem::path& path) : ShaderProgram({{GL_COMPUTE_SHADER, path}}) {}
};
