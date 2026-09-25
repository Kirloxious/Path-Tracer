#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "gpu/buffer.h"
#include "gpu/raster_shader.h"
#include "gpu/vertex_array.h"
#include "render/render_pass.h"

class RasterGBufferPass : public RenderPass
{
public:
    RasterGBufferPass(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath);

    void             onSceneLoaded(const Scene&) override;
    bool             reloadIfChanged() override;
    void             execute(const RenderContext&, RenderTargets&) override;
    std::string_view name() const override { return "Raster"; }

    /// Unused by execute(); kept for a future per-object draw, visibility toggle or multi-draw-indirect path.
    struct DrawRange
    {
        uint32_t objectId = 0;   ///< Or NO_OBJECT for unowned geometry.
        GLint    firstIndex = 0; ///< In indices.
        GLsizei  indexCount = 0; ///< In indices.
    };

    const std::vector<DrawRange>& getDrawRanges() const { return drawRanges; }

private:
    RasterShader shader;

    VertexArray vao;
    Buffer      vbo;
    Buffer      ebo;
    GLsizei     indexCount = 0;

    std::vector<DrawRange> drawRanges;

    /// Indices are grouped by object. The `flat` material_index attribute is only valid because every
    /// triangle sharing a vertex shares its material.
    void buildGeometry(const class World& world);
};
