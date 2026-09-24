#include "render/passes/path_tracer_pass.h"

#include <functional>
#include <initializer_list>

#include "core/log.h"
#include "gpu/buffer.h"

namespace {
// SSBO binding of each queue's index buffer, indexed by Q_*.
constexpr std::array<GLuint, NUM_QUEUES> QUEUE_BINDINGS = {
    BIND_RAY_QUEUE, BIND_HIT_OPAQUE_QUEUE, BIND_HIT_TRANSMISSIVE_QUEUE, BIND_HIT_EMISSIVE_QUEUE, BIND_SHADOW_QUEUE};

// prepare_indirect writes a uvec4 per queue; glDispatchComputeIndirect reads the first three.
constexpr GLintptr DISPATCH_ARG_STRIDE = 16;

// Counter slots each prepare_indirect dispatch zeroes once it has written their args.
// A slot can only be cleared after every kernel that reads it as a loop bound has run.
//
//   pass A runs before the shade kernels. ray and shadow were drained last iteration
//          (by trace and trace_shadow), so they are free to reset here; hit_* must
//          survive, because the shade kernels are about to read them as bounds.
//   pass B runs after the shade kernels. hit_* are now drained, while ray and shadow
//          have just been filled and are read by trace/trace_shadow below.
constexpr GLuint CLEAR_MASK_PRE_SHADE = (1u << Q_RAY) | (1u << Q_SHADOW);
constexpr GLuint CLEAR_MASK_POST_SHADE = (1u << Q_OPAQUE) | (1u << Q_TRANSMISSIVE) | (1u << Q_EMISSIVE);
} // namespace

PathTracerPass::PathTracerPass(int w, int h)
    : generate("shader/generate.comp"), trace("shader/trace.comp"), shadeOpaque("shader/shade_opaque.comp"),
      shadeTransmissive("shader/shade_transmissive.comp"), shadeEmissive("shader/shade_emissive.comp"), traceShadow("shader/trace_shadow.comp"),
      resolve("shader/resolve.comp"), prepareIndirect("shader/prepare_indirect.comp") {
    dispatchArgsSSBO = Buffer(nullptr, NUM_QUEUES * DISPATCH_ARG_STRIDE, GL_DYNAMIC_COPY);
    queueCounters = Buffer(nullptr, NUM_QUEUES * sizeof(uint32_t), GL_DYNAMIC_COPY);
    allocate(w, h);
}

void PathTracerPass::allocate(int w, int h) {
    const size_t numPixels = static_cast<size_t>(w) * static_cast<size_t>(h);
    Log::info("PathTracerPass (wavefront): {}x{} = {} pixels", w, h, numPixels);

    pathStateSSBO = Buffer(nullptr, numPixels * sizeof(PathState), GL_DYNAMIC_COPY);
    shadowStateSSBO = Buffer(nullptr, numPixels * sizeof(ShadowState), GL_DYNAMIC_COPY);
    for (Buffer& indices : queueIndices) {
        indices = Buffer(nullptr, numPixels * sizeof(uint32_t), GL_DYNAMIC_COPY);
    }
}

void PathTracerPass::resize(int w, int h) {
    allocate(w, h);
}

bool PathTracerPass::reloadIfChanged() {
    // Every kernel must be polled — `|=` on separate lines silently drifts when a new
    // stage is added, so enumerate them once here instead.
    const std::initializer_list<std::reference_wrapper<ComputeShader>> kernels = {
        generate, trace, shadeOpaque, shadeTransmissive, shadeEmissive, traceShadow, resolve, prepareIndirect};

    bool any = false;
    for (ComputeShader& kernel : kernels) {
        any |= kernel.reloadIfChanged();
    }
    return any;
}

void PathTracerPass::execute(const RenderContext& ctx, RenderTargets& targets) {
    const int maxBounces = ctx.camera.settings.max_bounces;

    glBindTextureUnit(TEX_GBUF_NORMAL, targets.gbuf.normal.id());
    glBindTextureUnit(TEX_GBUF_DEPTH, targets.gbuf.depth.id());

    pathStateSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_PATH_STATE);
    shadowStateSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_SHADOW_STATE);
    queueCounters.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_QUEUE_COUNTERS);
    for (size_t q = 0; q < queueIndices.size(); ++q) {
        queueIndices[q].bindBase(GL_SHADER_STORAGE_BUFFER, QUEUE_BINDINGS[q]);
    }
    dispatchArgsSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_DISPATCH_ARGS);
    dispatchArgsSSBO.bind(GL_DISPATCH_INDIRECT_BUFFER);

    // Every barrier below combines storage + indirect visibility so the next
    // glDispatchComputeIndirect can read the freshly-written args.
    constexpr GLbitfield BARRIER = GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT;

    queueCounters.clear();

    // ---- generate: gbuffer → hit_X queues ----
    generate.use();
    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(BARRIER);

    for (int b = 0; b < maxBounces; ++b) {
        // Rebuild the hit_* indirect args from the counters that generate (bounce 0) or the
        // previous iteration's trace just filled.
        prepareIndirect.use();
        prepareIndirect.setUInt("clear_mask", CLEAR_MASK_PRE_SHADE);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(BARRIER);

        // The shade kernels read disjoint hit_* queues and only append to ray/shadow, so they
        // are issued back-to-back behind one barrier.
        shadeOpaque.use();
        shadeOpaque.setInt("bounce_index", b);
        glDispatchComputeIndirect(Q_OPAQUE * DISPATCH_ARG_STRIDE);

        shadeTransmissive.use();
        shadeTransmissive.setInt("bounce_index", b);
        glDispatchComputeIndirect(Q_TRANSMISSIVE * DISPATCH_ARG_STRIDE);

        shadeEmissive.use();
        glDispatchComputeIndirect(Q_EMISSIVE * DISPATCH_ARG_STRIDE);

        glMemoryBarrier(BARRIER);

        prepareIndirect.use();
        prepareIndirect.setUInt("clear_mask", CLEAR_MASK_POST_SHADE);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(BARRIER);

        traceShadow.use();
        glDispatchComputeIndirect(Q_SHADOW * DISPATCH_ARG_STRIDE);
        glMemoryBarrier(BARRIER);

        // The last bounce has no next hit to route continuation rays into.
        if (b + 1 < maxBounces) {
            trace.use();
            glDispatchComputeIndirect(Q_RAY * DISPATCH_ARG_STRIDE);
            glMemoryBarrier(BARRIER);
        }
    }

    // ---- resolve: states[].radiance → accum, gbuffer normal → normals ----
    targets.accum.bindForAccumulation();
    targets.normals.bind(2, GL_WRITE_ONLY);
    targets.moments.bind(3, GL_READ_WRITE);

    resolve.use();
    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
