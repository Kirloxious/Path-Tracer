#pragma once

/**
 * @file compute_shader.h
 * @brief Compute-stage specialization of ShaderProgram.
 */

#include <filesystem>

#include "gpu/shader_program.h"

/**
 * @brief A single-stage compute program, hot-reloadable via ShaderProgram::reloadIfChanged().
 *
 * Compilation failure is logged and leaves `ID` at 0 (or, on a reload, keeps the previously
 * linked program) rather than throwing.
 */
class ComputeShader : public ShaderProgram
{
public:
    ComputeShader() = default;

    /**
     * @brief Loads, preprocesses `#include`s in, compiles and links a `.comp` source file.
     * @param path Path to the compute shader, resolved relative to the working directory —
     *             which is why the binary must be run from the project root.
     */
    explicit ComputeShader(const std::filesystem::path& path);

protected:
    /**
     * @brief Compiles the tracked `.comp` source and links it into a program.
     * @return A new program handle, or 0 if compilation or linking failed.
     */
    GLuint buildProgram() override;
};
