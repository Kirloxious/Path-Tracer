#include "raster_gbuffer_pass.h"

#include <cmath>
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

// UV-sphere tessellation. 24x12 is a good balance: 576 triangles per sphere,
// silhouette smooth enough that the (still-analytic) shadow rays the tracer
// casts against the true sphere SSBO hide any remaining facets.
constexpr int kSphereLongitude = 24;
constexpr int kSphereLatitude = 12;

void tessellateSphere(const Sphere& s, std::vector<RasterVertex>& out) {
    for (int lat = 0; lat < kSphereLatitude; ++lat) {
        float v0 = float(lat) / float(kSphereLatitude);
        float v1 = float(lat + 1) / float(kSphereLatitude);
        float phi0 = v0 * float(M_PI);
        float phi1 = v1 * float(M_PI);

        for (int lon = 0; lon < kSphereLongitude; ++lon) {
            float u0 = float(lon) / float(kSphereLongitude);
            float u1 = float(lon + 1) / float(kSphereLongitude);
            float th0 = u0 * 2.0f * float(M_PI);
            float th1 = u1 * 2.0f * float(M_PI);

            auto point = [&](float theta, float phi) {
                glm::vec3    n{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
                RasterVertex v;
                v.normal = n;
                v.pos = s.position + s.radius * n;
                v.matid = s.material_index;
                return v;
            };

            RasterVertex a = point(th0, phi0);
            RasterVertex b = point(th0, phi1);
            RasterVertex c = point(th1, phi1);
            RasterVertex d = point(th1, phi0);

            out.push_back(a);
            out.push_back(b);
            out.push_back(c);
            out.push_back(a);
            out.push_back(c);
            out.push_back(d);
        }
    }
}
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
    verts.reserve(world.triangles.size() * 3 + world.spheres.size() * kSphereLongitude * kSphereLatitude * 6);

    for (const Triangle& t : world.triangles) {
        glm::vec3 v0 = t.v0;
        glm::vec3 v1 = t.v0 + t.e1;
        glm::vec3 v2 = t.v0 + t.e2;
        verts.push_back({v0, t.n0, t.material_index});
        verts.push_back({v1, t.n1, t.material_index});
        verts.push_back({v2, t.n2, t.material_index});
    }
    for (const Sphere& s : world.spheres) {
        tessellateSphere(s, verts);
    }

    vertexCount = static_cast<GLsizei>(verts.size());
    if (vertexCount == 0) {
        Log::warn("RasterGBufferPass: no geometry to rasterize");
        return;
    }

    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);
    glNamedBufferData(vbo, verts.size() * sizeof(RasterVertex), verts.data(), GL_STATIC_DRAW);

    constexpr GLuint kBindingIndex = 0;
    glVertexArrayVertexBuffer(vao, kBindingIndex, vbo, 0, sizeof(RasterVertex));

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(RasterVertex, pos));
    glVertexArrayAttribBinding(vao, 0, kBindingIndex);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(RasterVertex, normal));
    glVertexArrayAttribBinding(vao, 1, kBindingIndex);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribIFormat(vao, 2, 1, GL_UNSIGNED_INT, offsetof(RasterVertex, matid));
    glVertexArrayAttribBinding(vao, 2, kBindingIndex);

    Log::info("RasterGBufferPass: {} vertices ({} triangles)", vertexCount, vertexCount / 3);
}

void RasterGBufferPass::uploadUniforms(const RenderContext& ctx) {
    buildGeometry(ctx.scene.world);
    prevViewProj = ctx.camera.data.projection * ctx.camera.data.view;
}

bool RasterGBufferPass::reloadIfChanged(const RenderContext&) {
    return shader.reloadIfChanged();
}

void RasterGBufferPass::execute(const RenderContext& ctx, RenderTargets& targets) {
    if (vertexCount == 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, targets.gbuf.fb.handle);
    glViewport(0, 0, targets.gbuf.width, targets.gbuf.height);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const float zeroPosMatid[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float zeroNormal[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float zeroMotion[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float clearDepth = 1.0f;
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_COLOR, GBuffer::ATTACH_POS_MATID, zeroPosMatid);
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_COLOR, GBuffer::ATTACH_NORMAL, zeroNormal);
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_COLOR, GBuffer::ATTACH_MOTION, zeroMotion);
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_DEPTH, 0, &clearDepth);

    shader.use();
    shader.setMat4("u_prev_view_proj", prevViewProj);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    prevViewProj = ctx.camera.data.projection * ctx.camera.data.view;
}
