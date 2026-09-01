#pragma once

/**
 * @file raster_gbuffer_pass.h
 * @brief Raster primary-visibility pass that fills the G-buffer.
 */

#include <filesystem>

#include "gpu/raster_shader.h"
#include "render/render_pass.h"

/**
 * @brief Rasterizes the scene into the G-buffer, replacing GPU primary ray casting.
 *
 * Issues a single `glDrawElements` over `World::vertices` plus an index buffer derived from
 * `Triangle::indices`, writing `gbuf.pos_matid` (xyz = world position, w =
 * `floatBitsToUint(material_index)`) and `gbuf.normal`. Face culling is disabled because the
 * Cornell-box scene mixes windings; the fragment shader flips normals via `gl_FrontFacing` to
 * match the path tracer's `set_face_normal` convention.
 *
 * execute() also swaps `targets.gbuf` with `targets.gbuf_prev` at entry, so this frame's draws
 * overwrite the frame-N-2 slot and frame N-1 survives in `gbuf_prev` for the temporal ReSTIR
 * pass. Nothing may be inserted between this pass and RestirPass that reads `gbuf_prev`.
 */
class RasterGBufferPass : public RenderPass
{
public:
    /**
     * @brief Loads the raster program.
     * @param vertPath Path to `gbuffer.vert`.
     * @param fragPath Path to `gbuffer.frag`.
     */
    RasterGBufferPass(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath);
    ~RasterGBufferPass() override;

    void        uploadUniforms(const Scene&, const Camera&) override;
    bool        reloadIfChanged(const RenderContext&) override;
    void        execute(const RenderContext&, RenderTargets&) override;
    const char* name() const override { return "Raster"; }

private:
    RasterShader shader;

    GLuint  vao = 0;
    GLuint  vbo = 0;
    GLuint  ebo = 0;
    GLsizei indexCount = 0;

    /**
     * @brief (Re)uploads the vertex and index buffers for a newly loaded world.
     *
     * The `material_index` vertex attribute is `flat`, which only round-trips correctly
     * because every triangle sharing a vertex shares its material — preserving that invariant
     * matters when adding new geometry builders.
     *
     * @param world Geometry to upload.
     */
    void buildGeometry(const class World& world);

    /// Deletes the VAO/VBO/EBO. Called before a rebuild and from the destructor.
    void releaseGeometry();
};
