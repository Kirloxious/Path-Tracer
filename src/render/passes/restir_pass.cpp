#include "render/passes/restir_pass.h"

#include "core/log.h"

namespace {
constexpr int    WORK_GROUP = 8;
constexpr GLuint BIND_RESERVOIRS_CURRENT = 18;
constexpr GLuint BIND_RESERVOIRS_PREV = 19;
constexpr GLuint BIND_RESERVOIRS_SPATIAL_INPUT = 20;
constexpr GLuint BIND_SURFACES_CURRENT = 24;
constexpr GLuint BIND_SURFACES_PREV = 25;
constexpr int    M_INITIAL_DEFAULT = 32;
// Cap on prev.M during temporal combine. ~20× M_initial is the standard
// Bitterli/Wyman recommendation: enough history to converge on static frames,
// short enough that lighting changes don't stick around for many frames.
constexpr int   M_CAP_DEFAULT = 20 * M_INITIAL_DEFAULT;
constexpr int   SPATIAL_K = 5;
constexpr float SPATIAL_RADIUS_PASS_1 = 30.0f;
constexpr float SPATIAL_RADIUS_PASS_2 = 15.0f;
} // namespace

RestirPass::RestirPass(int w, int h)
    : width(w), height(h), numPixels(w * h), numWorkGroupsX((w + WORK_GROUP - 1) / WORK_GROUP), numWorkGroupsY((h + WORK_GROUP - 1) / WORK_GROUP) {

    Log::info("RestirPass: {}x{} = {} pixels, reservoir SSBO = {} bytes (×2 for ping-pong)", w, h, numPixels, numPixels * sizeof(ReservoirData));

    initial = ComputeShader("shader/restir_initial.comp");
    temporal = ComputeShader("shader/restir_temporal.comp");
    spatial = ComputeShader("shader/restir_spatial.comp");

    const size_t bytes = numPixels * sizeof(ReservoirData);
    reservoirsA = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_CURRENT, nullptr, bytes, GL_DYNAMIC_COPY);
    reservoirsB = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_PREV, nullptr, bytes, GL_DYNAMIC_COPY);

    const size_t surfaceBytes = numPixels * sizeof(RestirSurfaceData);
    surfacesA = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_SURFACES_CURRENT, nullptr, surfaceBytes, GL_DYNAMIC_COPY);
    surfacesB = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_SURFACES_PREV, nullptr, surfaceBytes, GL_DYNAMIC_COPY);

    clearReservoirBuffers();
}

void RestirPass::clearReservoirBuffers() {
    // Zero both buffers. Reservoir layout has light_tri_idx at offset 0 — zero
    // there is a valid triangle index, but M=0 (zero) is the temporal-skip
    // signal in restir_temporal.comp, so the first frame correctly treats
    // both buffers as empty history.
    const uint32_t zero = 0u;
    glClearNamedBufferData(reservoirsA.id, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glClearNamedBufferData(reservoirsB.id, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    // Surfaces zero to valid = 0, which is exactly "this pixel has no resampling vertex",
    // so a cleared buffer reads as empty history rather than as a surface at the origin.
    glClearNamedBufferData(surfacesA.id, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glClearNamedBufferData(surfacesB.id, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
}

void RestirPass::uploadUniforms(const Scene& scene, const Camera&) {
    const int bvhRoot = scene.world.bvh.root;
    const int numLightGroups = static_cast<int>(scene.world.lightGroups.size());
    const int emissiveLast = scene.world.emissiveLastIndex;

    initial.use();
    initial.setInt("bvh_root_index", bvhRoot);
    initial.setInt("emissive_last_index", emissiveLast);
    initial.setInt("num_light_groups", numLightGroups);
    initial.setInt("m_initial", M_INITIAL_DEFAULT);

    temporal.use();
    temporal.setInt("bvh_root_index", bvhRoot);
    temporal.setInt("emissive_last_index", emissiveLast);
    temporal.setInt("m_cap", M_CAP_DEFAULT);

    spatial.use();
    spatial.setInt("bvh_root_index", bvhRoot);
    spatial.setInt("emissive_last_index", emissiveLast);
    spatial.setInt("k_neighbors", SPATIAL_K);

    // Scene loads and shader reloads invalidate any history we'd accumulated.
    clearReservoirBuffers();
}

void RestirPass::resize(int w, int h) {
    width = w;
    height = h;
    numPixels = w * h;
    numWorkGroupsX = (w + WORK_GROUP - 1) / WORK_GROUP;
    numWorkGroupsY = (h + WORK_GROUP - 1) / WORK_GROUP;

    const size_t bytes = numPixels * sizeof(ReservoirData);
    reservoirsA = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_CURRENT, nullptr, bytes, GL_DYNAMIC_COPY);
    reservoirsB = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_PREV, nullptr, bytes, GL_DYNAMIC_COPY);

    const size_t surfaceBytes = numPixels * sizeof(RestirSurfaceData);
    surfacesA = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_SURFACES_CURRENT, nullptr, surfaceBytes, GL_DYNAMIC_COPY);
    surfacesB = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_SURFACES_PREV, nullptr, surfaceBytes, GL_DYNAMIC_COPY);

    clearReservoirBuffers();
    useAAsCurrent = true;
}

bool RestirPass::reloadIfChanged(const RenderContext& ctx) {
    bool any = false;
    any |= initial.reloadIfChanged();
    any |= temporal.reloadIfChanged();
    any |= spatial.reloadIfChanged();
    if (any) {
        uploadUniforms(ctx.scene, ctx.camera);
    }
    return any;
}

void RestirPass::execute(const RenderContext& ctx, RenderTargets& targets) {
    // Rotate roles: this frame's writes go to whichever buffer was previously
    // "prev" (its frame N-2 contents are now stale). The buffer that held
    // frame N-1 reservoirs becomes "prev" so the temporal shader can sample it.
    const GLuint currentId = useAAsCurrent ? reservoirsA.id : reservoirsB.id;
    const GLuint prevId = useAAsCurrent ? reservoirsB.id : reservoirsA.id;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_CURRENT, currentId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_PREV, prevId);

    // Resampling surfaces rotate in lockstep with the reservoirs. Unlike the reservoirs,
    // the spatial passes never repurpose the prev buffer as scratch — they only read this
    // frame's surfaces — so these two bindings stay put for the whole pass.
    const GLuint surfCurrentId = useAAsCurrent ? surfacesA.id : surfacesB.id;
    const GLuint surfPrevId = useAAsCurrent ? surfacesB.id : surfacesA.id;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_SURFACES_CURRENT, surfCurrentId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_SURFACES_PREV, surfPrevId);

    // Bind current gbuffer as samplers 5/6 (raster pass already does this, but
    // rebinding keeps the pass self-contained), and the previous-frame gbuffer
    // as 7/8 for temporal validation.
    glBindTextureUnit(5, targets.gbuf.pos_matid.handle);
    glBindTextureUnit(6, targets.gbuf.normal.handle);
    glBindTextureUnit(7, targets.gbuf_prev.pos_matid.handle);
    glBindTextureUnit(8, targets.gbuf_prev.normal.handle);

    initial.use();
    initial.setIVec2("image_size", width, height);
    initial.setInt("frame_index", ctx.frameIndex);
    initial.setInt("time", static_cast<int>(ctx.timeSeed));
    glDispatchCompute(numWorkGroupsX, numWorkGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    temporal.use();
    temporal.setIVec2("image_size", width, height);
    temporal.setInt("frame_index", ctx.frameIndex);
    temporal.setInt("time", static_cast<int>(ctx.timeSeed));
    glDispatchCompute(numWorkGroupsX, numWorkGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Spatial reuse: two ping-ponged passes with shrinking radius. The shader
    // reads `spatial_input` (binding 20) and writes `reservoirs` (binding 18).
    // Buffer B (this frame's "prev") is repurposed as scratch — its contents
    // get overwritten by next frame's initial pass anyway.
    //
    // Pass 1: read currentId (post-temporal), write prevId (scratch).
    // Pass 2: read prevId (pass-1 output), write currentId. Final result lands
    // back in currentId at binding 18 for shade_lambertian to consume.
    spatial.use();
    spatial.setIVec2("image_size", width, height);
    spatial.setInt("frame_index", ctx.frameIndex);
    spatial.setInt("time", static_cast<int>(ctx.timeSeed));

    // Pass 1
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_SPATIAL_INPUT, currentId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_CURRENT, prevId);
    spatial.setFloat("radius_pixels", SPATIAL_RADIUS_PASS_1);
    spatial.setInt("pass_index", 0);
    // Pass 2 re-validates whatever sample survives, so this pass skips its shadow ray —
    // one fewer full-screen BVH traversal per frame (ReSTIR went from four to three).
    spatial.setInt("test_visibility", 0);
    glDispatchCompute(numWorkGroupsX, numWorkGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 2
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_SPATIAL_INPUT, prevId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_CURRENT, currentId);
    spatial.setFloat("radius_pixels", SPATIAL_RADIUS_PASS_2);
    spatial.setInt("pass_index", 1);
    // Final pass — its reservoir is what shade_lambertian consumes, so it must test.
    spatial.setInt("test_visibility", 1);
    glDispatchCompute(numWorkGroupsX, numWorkGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    useAAsCurrent = !useAAsCurrent;
}
