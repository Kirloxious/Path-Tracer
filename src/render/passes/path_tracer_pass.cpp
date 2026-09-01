#include "render/passes/path_tracer_pass.h"

#include <filesystem>
#include <functional>
#include <initializer_list>

#include "gpu/buffer.h"
#include "gpu/env_map.h"
#include "core/log.h"

namespace {
constexpr int WORK_GROUP_8x8 = 8;
constexpr int WORK_GROUP_64 = 64;

// SSBO binding numbers — must match shader/common/queue.glsl and shader/common/path_state.glsl.
constexpr GLuint BIND_PATH_STATE = 10;
constexpr GLuint BIND_QUEUE_COUNTERS = 11;
constexpr GLuint BIND_RAY_QUEUE_INDICES = 12;
constexpr GLuint BIND_HIT_OPAQUE_INDICES = 13;
constexpr GLuint BIND_HIT_TRANSMISSIVE_INDICES = 14;
constexpr GLuint BIND_HIT_EMISSIVE_INDICES = 15;
constexpr GLuint BIND_SHADOW_QUEUE_INDICES = 16;
constexpr GLuint BIND_SHADOW_STATE = 22;
constexpr GLuint BIND_DISPATCH_ARGS = 23;

// Counter-array slot indices — must match shader/common/queue.glsl Q_*.
constexpr int SLOT_RAY = 0;
constexpr int SLOT_OPAQUE = 1;
constexpr int SLOT_TRANSMISSIVE = 2;
constexpr int SLOT_EMISSIVE = 3;
constexpr int SLOT_SHADOW = 4;
constexpr int NUM_QUEUE_SLOTS = 5;

// prepare_indirect writes uvec4 per slot (uvec3 in std430 arrays has a 16-byte
// stride anyway; the .w padding just makes the layout explicit). glDispatchComputeIndirect
// reads 3 uints from `slot * DISPATCH_ARG_STRIDE` bytes.
constexpr GLintptr DISPATCH_ARG_STRIDE = 16;

// Counter slots each prepare_indirect dispatch zeroes once it has written their args.
// A slot can only be cleared after every kernel that reads it as a loop bound has run.
//
//   pass A runs before the shade kernels. ray and shadow were drained last iteration
//          (by trace and trace_shadow), so they are free to reset here; hit_* must
//          survive, because the shade kernels are about to read them as bounds.
//   pass B runs after the shade kernels. hit_* are now drained, while ray and shadow
//          have just been filled and are read by trace/trace_shadow below.
constexpr GLuint CLEAR_MASK_PRE_SHADE = (1u << SLOT_RAY) | (1u << SLOT_SHADOW);
constexpr GLuint CLEAR_MASK_POST_SHADE = (1u << SLOT_OPAQUE) | (1u << SLOT_TRANSMISSIVE) | (1u << SLOT_EMISSIVE);
} // namespace

PathTracerPass::PathTracerPass(int w, int h)
    : width(w), height(h), numPixels(w * h), numWorkGroupsX_8x8((w + WORK_GROUP_8x8 - 1) / WORK_GROUP_8x8),
      numWorkGroupsY_8x8((h + WORK_GROUP_8x8 - 1) / WORK_GROUP_8x8), numWorkGroups_64((w * h + WORK_GROUP_64 - 1) / WORK_GROUP_64) {

    Log::info("PathTracerPass (wavefront): {}x{} = {} pixels, {} 64-wide groups", w, h, numPixels, numWorkGroups_64);

    generate = ComputeShader("shader/generate.comp");
    trace = ComputeShader("shader/trace.comp");
    shadeOpaque = ComputeShader("shader/shade_opaque.comp");
    shadeTransmissive = ComputeShader("shader/shade_transmissive.comp");
    shadeEmissive = ComputeShader("shader/shade_emissive.comp");
    traceShadow = ComputeShader("shader/trace_shadow.comp");
    resolve = ComputeShader("shader/resolve.comp");
    prepareIndirect = ComputeShader("shader/prepare_indirect.comp");

    // PathState — width*height entries, 96 B each (was 144 B). NEE fields moved to shadowStateSSBO.
    pathStateSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_PATH_STATE, nullptr, numPixels * sizeof(PathState), GL_DYNAMIC_COPY);
    shadowStateSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_SHADOW_STATE, nullptr, numPixels * sizeof(ShadowState), GL_DYNAMIC_COPY);

    // NUM_QUEUE_SLOTS × 16 B (uvec4 stride in std430). Same GL buffer serves as SSBO (write, from
    // prepare_indirect) and GL_DISPATCH_INDIRECT_BUFFER (read, by glDispatchComputeIndirect).
    dispatchArgsSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_DISPATCH_ARGS, nullptr, NUM_QUEUE_SLOTS * DISPATCH_ARG_STRIDE, GL_DYNAMIC_COPY);

    queueCounters = QueueCounters(BIND_QUEUE_COUNTERS, NUM_QUEUE_SLOTS);

    // Each Queue below stores `&queueCounters` — a pointer into *this* object. That is only
    // safe because RenderPass deletes copy and declares no move, and passes are always held
    // via unique_ptr, so a PathTracerPass never changes address. Adding a move constructor
    // here would leave every Queue pointing at the moved-from husk.
    //
    // Each queue's `indices` is sized for the worst case (every pixel in this queue).
    rayQueue = Queue(queueCounters, SLOT_RAY, BIND_RAY_QUEUE_INDICES, numPixels);
    hitOpaque = Queue(queueCounters, SLOT_OPAQUE, BIND_HIT_OPAQUE_INDICES, numPixels);
    hitTransmissive = Queue(queueCounters, SLOT_TRANSMISSIVE, BIND_HIT_TRANSMISSIVE_INDICES, numPixels);
    hitEmissive = Queue(queueCounters, SLOT_EMISSIVE, BIND_HIT_EMISSIVE_INDICES, numPixels);
    shadowQueue = Queue(queueCounters, SLOT_SHADOW, BIND_SHADOW_QUEUE_INDICES, numPixels);
}

void PathTracerPass::uploadUniforms(const Scene& scene, const Camera& camera) {
    // Static (per-scene) uniforms. Per-frame uniforms are set in execute().
    const int bvhRoot = scene.world.bvh.root;
    // Only the kernels that actually call is_visible() get this: elsewhere the uniform is
    // dead-stripped by the compiler and setting it would log a spurious "not found".
    const int   emissiveLast = scene.world.emissiveLastIndex;
    const int   numLightGroups = static_cast<int>(scene.world.lightGroups.size());
    const int   maxBounces = camera.settings.max_bounces;
    const bool  envValid = !scene.envMapPath.empty();
    const float envIntensity = scene.envIntensity;
    const float indirectClamp = camera.settings.indirect_clamp;

    // Uniform locations come from shader/common/uniform_locations.glsl; the named
    // setters go through ShaderProgram's location cache, so there is no reason to
    // hand-write the raw glUniform*(location, ...) calls these used to use.
    trace.use();
    trace.setInt("bvh_root_index", bvhRoot);
    trace.setInt("env_map_valid", envValid ? 1 : 0);
    trace.setFloat("env_map_intensity", envIntensity);
    trace.setFloat("indirect_clamp", indirectClamp);

    generate.use();
    generate.setInt("env_map_valid", envValid ? 1 : 0);
    generate.setFloat("env_map_intensity", envIntensity);

    traceShadow.use();
    traceShadow.setInt("bvh_root_index", bvhRoot);
    traceShadow.setInt("emissive_last_index", emissiveLast);

    shadeOpaque.use();
    shadeOpaque.setInt("num_light_groups", numLightGroups);
    shadeOpaque.setInt("max_bounces", maxBounces);
    shadeOpaque.setFloat("indirect_clamp", indirectClamp);

    shadeEmissive.use();
    shadeEmissive.setInt("num_light_groups", numLightGroups);
    shadeEmissive.setFloat("indirect_clamp", indirectClamp);

    shadeTransmissive.use();
    shadeTransmissive.setInt("max_bounces", maxBounces);
}

void PathTracerPass::resize(int w, int h) {
    width = w;
    height = h;
    numPixels = w * h;
    numWorkGroupsX_8x8 = (w + WORK_GROUP_8x8 - 1) / WORK_GROUP_8x8;
    numWorkGroupsY_8x8 = (h + WORK_GROUP_8x8 - 1) / WORK_GROUP_8x8;
    numWorkGroups_64 = (numPixels + WORK_GROUP_64 - 1) / WORK_GROUP_64;

    pathStateSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_PATH_STATE, nullptr, numPixels * sizeof(PathState), GL_DYNAMIC_COPY);
    shadowStateSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_SHADOW_STATE, nullptr, numPixels * sizeof(ShadowState), GL_DYNAMIC_COPY);

    // dispatchArgsSSBO is size-independent (6 slots, fixed) but the queues below
    // scale with pixel count, so they must be reallocated. Rebind their counter
    // to the same shared QueueCounters (still valid, unchanged).
    rayQueue = Queue(queueCounters, SLOT_RAY, BIND_RAY_QUEUE_INDICES, numPixels);
    hitOpaque = Queue(queueCounters, SLOT_OPAQUE, BIND_HIT_OPAQUE_INDICES, numPixels);
    hitTransmissive = Queue(queueCounters, SLOT_TRANSMISSIVE, BIND_HIT_TRANSMISSIVE_INDICES, numPixels);
    hitEmissive = Queue(queueCounters, SLOT_EMISSIVE, BIND_HIT_EMISSIVE_INDICES, numPixels);
    shadowQueue = Queue(queueCounters, SLOT_SHADOW, BIND_SHADOW_QUEUE_INDICES, numPixels);
}

bool PathTracerPass::reloadIfChanged(const RenderContext& ctx) {
    // Every kernel must be polled — `|=` on separate lines silently drifts when a new
    // stage is added, so enumerate them once here instead.
    const std::initializer_list<std::reference_wrapper<ComputeShader>> kernels = {
        generate, trace, shadeOpaque, shadeTransmissive, shadeEmissive, traceShadow, resolve, prepareIndirect};

    bool any = false;
    for (ComputeShader& kernel : kernels) {
        any |= kernel.reloadIfChanged();
    }
    if (any) {
        uploadUniforms(ctx.scene, ctx.camera);
    }
    return any;
}

void PathTracerPass::execute(const RenderContext& ctx, RenderTargets& targets) {
    const int maxBounces = ctx.camera.settings.max_bounces;

    // ---- Bind read-only inputs from the raster gbuffer ----
    glBindTextureUnit(6, targets.gbuf.normal.handle);
    glBindTextureUnit(10, targets.gbuf.depth.handle); // world position is reconstructed from this

    // Envmap texture at unit 9 (envmap.glsl declares binding=9). Bind unconditionally —
    // if the scene has no envmap, sample_envmap() returns black via the env_map_valid uniform.
    if (targets.envMap && targets.envMap->valid()) {
        targets.envMap->bind(9);
    }

    // Bind the indirect args buffer once. It's still bound as an SSBO at
    // BIND_DISPATCH_ARGS for prepareIndirect's writes.
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatchArgsSSBO.id);

    // Every barrier below combines storage + indirect visibility so the next
    // glDispatchComputeIndirect can read the freshly-written args.
    constexpr GLbitfield BARRIER = GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT;

    // ---- Clear all queue counters ----
    queueCounters.clearAll();

    // ---- generate: gbuffer → hit_X queues ----
    generate.use();
    generate.setIVec2("image_size", width, height);
    generate.setInt("frame_index", ctx.frameIndex);
    generate.setInt("time", static_cast<int>(ctx.timeSeed));
    glDispatchCompute(numWorkGroupsX_8x8, numWorkGroupsY_8x8, 1);
    glMemoryBarrier(BARRIER);

    // ---- Bounce loop ----
    for (int b = 0; b < maxBounces; ++b) {
        // Rebuild hit_{L,M,D,E} indirect args from the queue counters that generate
        // (bounce 0) or the previous iteration's trace (bounce > 0) just filled.
        prepareIndirect.use();
        prepareIndirect.setUInt("clear_mask", CLEAR_MASK_PRE_SHADE);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(BARRIER);

        // Shade. Each kernel reads its own hit_X queue and writes ray_queue + shadow_queue
        // via atomicAdd; they don't depend on each other's outputs, so we issue all three
        // back-to-back and barrier once at the end.
        shadeOpaque.use();
        shadeOpaque.setInt("bounce_index", b);
        glDispatchComputeIndirect(SLOT_OPAQUE * DISPATCH_ARG_STRIDE);

        shadeTransmissive.use();
        shadeTransmissive.setInt("bounce_index", b);
        glDispatchComputeIndirect(SLOT_TRANSMISSIVE * DISPATCH_ARG_STRIDE);

        shadeEmissive.use();
        glDispatchComputeIndirect(SLOT_EMISSIVE * DISPATCH_ARG_STRIDE);

        glMemoryBarrier(BARRIER);

        // Refresh args for the shadow_queue and ray_queue that shades just filled, and reset
        // the hit_* counters the shades have now drained.
        prepareIndirect.use();
        prepareIndirect.setUInt("clear_mask", CLEAR_MASK_POST_SHADE);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(BARRIER);

        // Shadow visibility. Reads shadow_queue + shadow_states + states, writes states[].radiance.
        traceShadow.use();
        glDispatchComputeIndirect(SLOT_SHADOW * DISPATCH_ARG_STRIDE);
        glMemoryBarrier(BARRIER);

        // Trace continuation rays (skip on the last bounce — there's no "next" hit to write).
        if (b + 1 < maxBounces) {
            trace.use();
            glDispatchComputeIndirect(SLOT_RAY * DISPATCH_ARG_STRIDE);
            glMemoryBarrier(BARRIER);
        }
    }

    // ---- resolve: states[].radiance → accum_image, gbuffer normal → normals_image ----
    targets.accum.bindForAccumulation();
    targets.normals.bind(2, GL_WRITE_ONLY);

    resolve.use();
    resolve.setIVec2("image_size", width, height);
    resolve.setInt("frame_index", ctx.frameIndex);
    glDispatchCompute(numWorkGroupsX_8x8, numWorkGroupsY_8x8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
