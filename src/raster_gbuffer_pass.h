#pragma once

#include <filesystem>

#include "raster_shader.h"
#include "render_pass.h"

class RasterGBufferPass : public RenderPass
{
public:
    RasterGBufferPass(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath);
    ~RasterGBufferPass() override;

    void uploadUniforms(const RenderContext&) override;
    bool reloadIfChanged(const RenderContext&) override;
    void execute(const RenderContext&, RenderTargets&) override;

private:
    RasterShader shader;

    GLuint  vao = 0;
    GLuint  vbo = 0;
    GLuint  ebo = 0;
    GLsizei indexCount = 0;

    void buildGeometry(const class World& world);
    void releaseGeometry();
};
