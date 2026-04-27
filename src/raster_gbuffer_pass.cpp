#include "raster_gbuffer_pass.h"

#include <vector>

#include "log.h"
#include "primitive.h"
#include "world.h"

RasterGBufferPass::RasterGBufferPass(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath) {
    Log::info("RasterGBufferPass: loading '{}' + '{}'", vertPath.string(), fragPath.string());
    shader = RasterShader(vertPath, fragPath);
}

RasterGBufferPass::~RasterGBufferPass() {
    releaseGeometry();
}

void RasterGBufferPass::releaseGeometry() {
    if (ebo) {
        glDeleteBuffers(1, &ebo);
        ebo = 0;
    }
    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    indexCount = 0;
}

void RasterGBufferPass::buildGeometry(const World& world) {
    releaseGeometry();

    if (world.vertices.empty() || world.triangles.empty()) {
        Log::warn("RasterGBufferPass: no geometry to rasterize");
        return;
    }

    std::vector<uint32_t> indices;
    indices.reserve(world.triangles.size() * 3);
    for (const Triangle& t : world.triangles) {
        indices.push_back(t.indices.x);
        indices.push_back(t.indices.y);
        indices.push_back(t.indices.z);
    }

    indexCount = static_cast<GLsizei>(indices.size());

    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);
    glCreateBuffers(1, &ebo);

    glNamedBufferData(vbo, world.vertices.size() * sizeof(Vertex), world.vertices.data(), GL_STATIC_DRAW);
    glNamedBufferData(ebo, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    constexpr GLuint bindingIndex = 0;
    glVertexArrayVertexBuffer(vao, bindingIndex, vbo, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(vao, ebo);

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribBinding(vao, 0, bindingIndex);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribBinding(vao, 1, bindingIndex);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribIFormat(vao, 2, 1, GL_UNSIGNED_INT, offsetof(Vertex, material_index));
    glVertexArrayAttribBinding(vao, 2, bindingIndex);

    Log::info("RasterGBufferPass: {} vertices, {} indices ({} triangles)", world.vertices.size(), indexCount, indexCount / 3);
}

void RasterGBufferPass::uploadUniforms(const RenderContext& ctx) {
    buildGeometry(ctx.scene.world);
}

bool RasterGBufferPass::reloadIfChanged(const RenderContext&) {
    return shader.reloadIfChanged();
}

void RasterGBufferPass::execute(const RenderContext&, RenderTargets& targets) {
    if (indexCount == 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, targets.gbuf.fb.handle);
    glViewport(0, 0, targets.gbuf.width, targets.gbuf.height);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    // Scenes here don't enforce a consistent winding (e.g. Cornell-box lids wind inward),
    // so we draw both sides and let the fragment shader flip normals via gl_FrontFacing —
    // mirroring the path tracer's own set_face_normal convention.
    glDisable(GL_CULL_FACE);

    const float zeroPosMatid[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float zeroNormal[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float clearDepth = 1.0f;
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_COLOR, GBuffer::ATTACH_POS_MATID, zeroPosMatid);
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_COLOR, GBuffer::ATTACH_NORMAL, zeroNormal);
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_DEPTH, 0, &clearDepth);

    shader.use();

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    targets.gbuf.pos_matid.bind(5, GL_READ_ONLY);
    targets.gbuf.normal.bind(6, GL_READ_ONLY);
}
