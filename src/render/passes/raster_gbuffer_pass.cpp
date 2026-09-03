#include "render/passes/raster_gbuffer_pass.h"

#include <utility>
#include <vector>

#include "core/log.h"
#include "scene/primitive.h"
#include "scene/world.h"

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
    drawRanges.clear();
}

void RasterGBufferPass::buildGeometry(const World& world) {
    releaseGeometry();

    if (world.vertices.empty() || world.triangles.empty()) {
        Log::warn("RasterGBufferPass: no geometry to rasterize");
        return;
    }

    // Gives each object one contiguous run; NO_OBJECT triangles land in a trailing bucket.
    const std::size_t objectCount = world.objects.size();
    const std::size_t bucketCount = objectCount + 1;
    const std::size_t unownedBucket = objectCount;

    auto bucketOf = [&](std::size_t triIndex) -> std::size_t {
        if (triIndex >= world.triangleObjectId.size()) {
            return unownedBucket;
        }
        const uint32_t id = world.triangleObjectId[triIndex];
        return (id < objectCount) ? static_cast<std::size_t>(id) : unownedBucket;
    };

    std::vector<uint32_t> triCounts(bucketCount, 0);
    for (std::size_t i = 0; i < world.triangles.size(); ++i) {
        ++triCounts[bucketOf(i)];
    }

    std::vector<uint32_t> cursor(bucketCount, 0);
    uint32_t              running = 0;
    drawRanges.clear();
    drawRanges.reserve(bucketCount);
    for (std::size_t b = 0; b < bucketCount; ++b) {
        cursor[b] = running;
        if (triCounts[b] > 0) {
            DrawRange range;
            range.objectId = (b == unownedBucket) ? NO_OBJECT : static_cast<uint32_t>(b);
            range.firstIndex = static_cast<GLint>(running * 3);
            range.indexCount = static_cast<GLsizei>(triCounts[b] * 3);
            drawRanges.push_back(range);
        }
        running += triCounts[b];
    }

    std::vector<uint32_t> indices(world.triangles.size() * 3);
    for (std::size_t i = 0; i < world.triangles.size(); ++i) {
        const Triangle&   t = world.triangles[i];
        const std::size_t slot = cursor[bucketOf(i)]++;
        indices[slot * 3 + 0] = t.indices.x;
        indices[slot * 3 + 1] = t.indices.y;
        indices[slot * 3 + 2] = t.indices.z;
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

    Log::info("RasterGBufferPass: {} vertices, {} indices ({} triangles) across {} object run(s)",
              world.vertices.size(),
              indexCount,
              indexCount / 3,
              drawRanges.size());
}

void RasterGBufferPass::uploadUniforms(const Scene& scene, const Camera&) {
    buildGeometry(scene.world);
}

bool RasterGBufferPass::reloadIfChanged(const RenderContext&) {
    return shader.reloadIfChanged();
}

void RasterGBufferPass::execute(const RenderContext&, RenderTargets& targets) {
    if (indexCount == 0) {
        return;
    }

    // Rotate the gbuffer pair: gbuf_prev becomes the slot we draw into this frame
    // (overwriting frame N-2 data), and the previous gbuf moves into gbuf_prev,
    // preserving frame N-1 data for temporal consumers downstream.
    std::swap(targets.gbuf, targets.gbuf_prev);

    glBindFramebuffer(GL_FRAMEBUFFER, targets.gbuf.fb.handle);
    glViewport(0, 0, targets.gbuf.width, targets.gbuf.height);

    glEnable(GL_DEPTH_TEST);
    // Reversed-Z: the projection maps far to 0 and near to 1, so "closer" is now "greater".
    // Must stay in lockstep with makeReversedZProjection() and the 0.0 depth clear below.
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    // Scenes here don't enforce a consistent winding (e.g. Cornell-box lids wind inward), so
    // we draw both sides and let the fragment shader flip normals against the view direction —
    // mirroring the path tracer's own set_face_normal convention.
    glDisable(GL_CULL_FACE);

    const float zeroNormal[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float clearDepth = 0.0f; // reversed-Z: 0 is the far plane
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_COLOR, GBuffer::ATTACH_NORMAL, zeroNormal);
    glClearNamedFramebufferfv(targets.gbuf.fb.handle, GL_DEPTH, 0, &clearDepth);

    shader.use();

    glBindVertexArray(vao);
    // `drawRanges` tiles the buffer exactly, so a draw per object would submit the same
    // triangles at N times the CPU cost.
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}
