#pragma once

#include <filesystem>
#include <glm/glm.hpp>

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

    GLuint    vao = 0;
    GLuint    vbo = 0;
    GLsizei   vertexCount = 0;
    glm::mat4 prevViewProj = glm::mat4(1.0f);

    void buildGeometry(const class World& world);
    void releaseGeometry();
};
