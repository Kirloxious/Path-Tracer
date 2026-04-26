#include "raster_gbuffer_pass.h"

#include <vector>

#include "log.h"
#include "primitive.h"
#include "world.h"

namespace {
struct RasterVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    uint32_t  matid;
};
static_assert(sizeof(RasterVertex) == 28, "RasterVertex must be tightly packed (28 bytes)");
} // namespace

RasterGBufferPass::RasterGBufferPass(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath) {
    Log::info("RasterGBufferPass: loading '{}' + '{}'", vertPath.string(), fragPath.string());
    shader = RasterShader(vertPath, fragPath);
}

RasterGBufferPass::~RasterGBufferPass() {
    releaseGeometry();
}

void RasterGBufferPass::releaseGeometry() {
    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    vertexCount = 0;
}

void RasterGBufferPass::buildGeometry(const World& world) {
    releaseGeometry();

    std::vector<RasterVertex> verts;
    verts.reserve(world.triangles.size() * 3);

    for (const Triangle& t : world.triangles) {
        glm::vec3 v0 = t.v0;
        glm::vec3 v1 = t.v0 + t.e1;
        glm::vec3 v2 = t.v0 + t.e2;
        verts.push_back({v0, t.n0, t.material_index});
        verts.push_back({v1, t.n1, t.material_index});
        verts.push_back({v2, t.n2, t.material_index});
    }

    vertexCount = static_cast<GLsizei>(verts.size());
    if (vertexCount == 0) {
        Log::warn("RasterGBufferPass: no geometry to rasterize");
        return;
    }

    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);
    glNamedBufferData(vbo, verts.size() * sizeof(RasterVertex), verts.data(), GL_STATIC_DRAW);

    constexpr GLuint bindingIndex = 0;
    glVertexArrayVertexBuffer(vao, bindingIndex, vbo, 0, sizeof(RasterVertex));

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(RasterVertex, pos));
    glVertexArrayAttribBinding(vao, 0, bindingIndex);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(RasterVertex, normal));
    glVertexArrayAttribBinding(vao, 1, bindingIndex);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribIFormat(vao, 2, 1, GL_UNSIGNED_INT, offsetof(RasterVertex, matid));
    glVertexArrayAttribBinding(vao, 2, bindingIndex);

    Log::info("RasterGBufferPass: {} vertices ({} triangles)", vertexCount, vertexCount / 3);
}

void RasterGBufferPass::uploadUniforms(const RenderContext& ctx) {
    buildGeometry(ctx.scene.world);
}

bool RasterGBufferPass::reloadIfChanged(const RenderContext&) {
    return shader.reloadIfChanged();
}

void RasterGBufferPass::execute(const RenderContext&, RenderTargets& targets) {
    if (vertexCount == 0) {
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
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    targets.gbuf.pos_matid.bind(5, GL_READ_ONLY);
    targets.gbuf.normal.bind(6, GL_READ_ONLY);
}
