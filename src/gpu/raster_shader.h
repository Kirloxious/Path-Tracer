#pragma once

/**
 * @file raster_shader.h
 * @brief Vertex + fragment specialization of ShaderProgram.
 */

#include <filesystem>

#include "gpu/shader_program.h"

/**
 * @brief A vertex + fragment program, hot-reloadable via ShaderProgram::reloadIfChanged().
 *
 * Used only by RasterGBufferPass. Compilation failure is logged and keeps the previously
 * linked program (or leaves `ID` at 0 on first build) rather than throwing.
 */
class RasterShader : public ShaderProgram
{
public:
    RasterShader() = default;

    /**
     * @brief Loads, preprocesses, compiles and links a vertex/fragment pair.
     * @param vertPath Path to the `.vert` source.
     * @param fragPath Path to the `.frag` source.
     */
    RasterShader(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath);

protected:
    /**
     * @brief Compiles both tracked stages and links them into a program.
     * @return A new program handle, or 0 if either stage failed to compile or the link failed.
     */
    GLuint buildProgram() override;
};
