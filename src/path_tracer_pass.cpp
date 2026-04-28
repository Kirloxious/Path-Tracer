#include "path_tracer_pass.h"

#include <filesystem>

#include "buffer.h"
#include "log.h"

namespace {
constexpr int WORK_GROUP_8x8 = 8;
constexpr int WORK_GROUP_64 = 64;

// SSBO binding numbers — must match shader/common/queue.glsl and shader/common/path_state.glsl.
constexpr GLuint BIND_PATH_STATE = 10;
constexpr GLuint BIND_QUEUE_COUNTERS = 11;
constexpr GLuint BIND_RAY_QUEUE_INDICES = 12;
constexpr GLuint BIND_HIT_LAMBERTIAN_INDICES = 13;
constexpr GLuint BIND_HIT_METAL_INDICES = 14;
constexpr GLuint BIND_HIT_DIELECTRIC_INDICES = 15;
constexpr GLuint BIND_HIT_EMISSIVE_INDICES = 16;
constexpr GLuint BIND_SHADOW_QUEUE_INDICES = 17;

// Counter-array slot indices — must match shader/common/queue.glsl Q_*.
constexpr int SLOT_RAY = 0;
constexpr int SLOT_LAMB = 1;
constexpr int SLOT_METAL = 2;
constexpr int SLOT_DIELECTRIC = 3;
constexpr int SLOT_EMISSIVE = 4;
constexpr int SLOT_SHADOW = 5;
constexpr int NUM_QUEUE_SLOTS = 6;
} // namespace

PathTracerPass::PathTracerPass(int w, int h)
    : width(w), height(h), numPixels(w * h), numWorkGroupsX_8x8((w + WORK_GROUP_8x8 - 1) / WORK_GROUP_8x8),
      numWorkGroupsY_8x8((h + WORK_GROUP_8x8 - 1) / WORK_GROUP_8x8), numWorkGroups_64((w * h + WORK_GROUP_64 - 1) / WORK_GROUP_64) {

    Log::info("PathTracerPass (wavefront): {}x{} = {} pixels, {} 64-wide groups", w, h, numPixels, numWorkGroups_64);

    generate = ComputeShader("shader/generate.comp");
    trace = ComputeShader("shader/trace.comp");
    shadeLambertian = ComputeShader("shader/shade_lambertian.comp");
    shadeMetal = ComputeShader("shader/shade_metal.comp");
    shadeDielectric = ComputeShader("shader/shade_dielectric.comp");
    shadeEmissive = ComputeShader("shader/shade_emissive.comp");
    traceShadow = ComputeShader("shader/trace_shadow.comp");
    resolve = ComputeShader("shader/resolve.comp");

    // PathState — width*height entries, 144 B each. ~117 MB at 1200x675.
    pathStateSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_PATH_STATE, nullptr, numPixels * sizeof(PathState), GL_DYNAMIC_COPY);

    queueCounters = QueueCounters(BIND_QUEUE_COUNTERS, NUM_QUEUE_SLOTS);

    // Each queue's `indices` is sized for the worst case (every pixel in this queue).
    rayQueue = Queue(queueCounters, SLOT_RAY, BIND_RAY_QUEUE_INDICES, numPixels);
    hitLambertian = Queue(queueCounters, SLOT_LAMB, BIND_HIT_LAMBERTIAN_INDICES, numPixels);
    hitMetal = Queue(queueCounters, SLOT_METAL, BIND_HIT_METAL_INDICES, numPixels);
    hitDielectric = Queue(queueCounters, SLOT_DIELECTRIC, BIND_HIT_DIELECTRIC_INDICES, numPixels);
    hitEmissive = Queue(queueCounters, SLOT_EMISSIVE, BIND_HIT_EMISSIVE_INDICES, numPixels);
    shadowQueue = Queue(queueCounters, SLOT_SHADOW, BIND_SHADOW_QUEUE_INDICES, numPixels);
}

void PathTracerPass::uploadUniforms(const RenderContext& ctx) {
    // Static (per-scene) uniforms. Per-frame uniforms are set in execute().
    const int bvhRoot = ctx.scene.world.bvh.root;
    const int numLightGroups = static_cast<int>(ctx.scene.world.lightGroups.size());
    const int maxBounces = ctx.camera.settings.max_bounces;

    trace.use();
    trace.setInt("bvh_root_index", bvhRoot);

    traceShadow.use();
    traceShadow.setInt("bvh_root_index", bvhRoot);

    shadeLambertian.use();
    shadeLambertian.setInt("num_light_groups", numLightGroups);
    shadeLambertian.setInt("max_bounces", maxBounces);

    shadeMetal.use();
    shadeMetal.setInt("max_bounces", maxBounces);

    shadeDielectric.use();
    shadeDielectric.setInt("max_bounces", maxBounces);
}

bool PathTracerPass::reloadIfChanged(const RenderContext& ctx) {
    bool any = false;
    any |= generate.reloadIfChanged();
    any |= trace.reloadIfChanged();
    any |= shadeLambertian.reloadIfChanged();
    any |= shadeMetal.reloadIfChanged();
    any |= shadeDielectric.reloadIfChanged();
    any |= shadeEmissive.reloadIfChanged();
    any |= traceShadow.reloadIfChanged();
    any |= resolve.reloadIfChanged();
    if (any) {
        uploadUniforms(ctx);
    }
    return any;
}

void PathTracerPass::execute(const RenderContext& ctx, RenderTargets& targets) {
    const int maxBounces = ctx.camera.settings.max_bounces;

    // ---- Bind read-only inputs from the raster gbuffer ----
    glBindTextureUnit(5, targets.gbuf.pos_matid.handle);
    glBindTextureUnit(6, targets.gbuf.normal.handle);

    // ---- Clear all queue counters ----
    queueCounters.clearAll();

    // ---- generate: gbuffer → hit_X queues ----
    generate.use();
    glUniform2i(glGetUniformLocation(generate.ID, "image_size"), width, height);
    generate.setInt("frame_index", ctx.frameIndex);
    glDispatchCompute(numWorkGroupsX_8x8, numWorkGroupsY_8x8, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ---- Bounce loop ----
    for (int b = 0; b < maxBounces; ++b) {
        // Shade. Each kernel reads its own hit_X queue and writes ray_queue + shadow_queue
        // via atomicAdd; they don't depend on each other's outputs, so we issue all four
        // back-to-back and barrier once at the end.
        shadeLambertian.use();
        shadeLambertian.setInt("bounce_index", b);
        glDispatchCompute(numWorkGroups_64, 1, 1);

        shadeMetal.use();
        shadeMetal.setInt("bounce_index", b);
        glDispatchCompute(numWorkGroups_64, 1, 1);

        shadeDielectric.use();
        shadeDielectric.setInt("bounce_index", b);
        glDispatchCompute(numWorkGroups_64, 1, 1);

        shadeEmissive.use();
        glDispatchCompute(numWorkGroups_64, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Shadow visibility. Reads shadow_queue + states, writes states[].radiance.
        traceShadow.use();
        glDispatchCompute(numWorkGroups_64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Hit_X consumed by shade; shadow consumed by traceShadow. Reset for next iter.
        hitLambertian.clear();
        hitMetal.clear();
        hitDielectric.clear();
        hitEmissive.clear();
        shadowQueue.clear();

        // Trace continuation rays (skip on the last bounce — there's no "next" hit to write).
        if (b + 1 < maxBounces) {
            trace.use();
            glDispatchCompute(numWorkGroups_64, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            rayQueue.clear();
        }
    }

    // ---- resolve: states[].radiance → accum_image, gbuffer normal → normals_image ----
    targets.accum.bindForAccumulation();
    targets.normals.bind(2, GL_WRITE_ONLY);

    resolve.use();
    glUniform2i(glGetUniformLocation(resolve.ID, "image_size"), width, height);
    resolve.setInt("frame_index", ctx.frameIndex);
    glDispatchCompute(numWorkGroupsX_8x8, numWorkGroupsY_8x8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
