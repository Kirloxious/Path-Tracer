#pragma once

/**
 * @file raster_gbuffer_pass.h
 * @brief Raster primary-visibility pass that fills the G-buffer.
 */

#include <cstdint>
#include <filesystem>
#include <vector>

#include "gpu/raster_shader.h"
#include "render/render_pass.h"

/**
 * @brief Rasterizes the scene into the G-buffer, replacing GPU primary ray casting.
 *
 * Issues a single `glDrawElements` over `World::vertices` plus an index buffer derived from
 * `Triangle::indices`, writing `gbuf.normal` (xyz = world normal, w = `float(material_index)`)
 * and depth. Face culling is disabled because the Cornell-box scene mixes windings; the
 * fragment shader flips normals against the view direction to match the path tracer's
 * `set_face_normal` convention.
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

    /**
     * @brief One object's contiguous slice of the index buffer.
     *
     * Unused by execute(), which draws the whole buffer at once. They exist because a
     * per-object draw, a visibility toggle and a glMultiDrawElementsIndirect path all need
     * this layout, and grouping costs one counting sort at scene load.
     */
    struct DrawRange
    {
        uint32_t objectId = 0;   ///< Index into World::objects, or NO_OBJECT for unowned geometry.
        GLint    firstIndex = 0; ///< Offset of the run's first index, in indices.
        GLsizei  indexCount = 0; ///< Length of the run, in indices (3 per triangle).
    };

    /// @return Per-object index-buffer runs, in object order. Empty until a world is loaded.
    const std::vector<DrawRange>& getDrawRanges() const { return drawRanges; }

private:
    RasterShader shader;

    GLuint  vao = 0;
    GLuint  vbo = 0;
    GLuint  ebo = 0;
    GLsizei indexCount = 0;

    std::vector<DrawRange> drawRanges;

    /**
     * @brief (Re)uploads the vertex and index buffers for a newly loaded world.
     *
     * Indices are emitted grouped by `World::triangleObjectId` rather than in triangle order,
     * and each object's run is recorded in `drawRanges`. Only submission order changes; the
     * depth test still decides visibility.
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
