#include "render/passes/path_tracer_pass.h"

#include <functional>
#include <initializer_list>

#include "core/log.h"
#include "gpu/buffer.h"
#include "gpu/gl.h"

namespace {
constexpr std::array<GLuint, NUM_QUEUES> QUEUE_BINDINGS = {
    BIND_RAY_QUEUE, BIND_HIT_OPAQUE_QUEUE, BIND_HIT_TRANSMISSIVE_QUEUE, BIND_HIT_EMISSIVE_QUEUE, BIND_SHADOW_QUEUE};

constexpr GLintptr DISPATCH_ARG_STRIDE = 16;

// Counter slots each prepare_indirect pass zeroes; a slot clears only once its readers have run.
// Pass A (before shade) resets ray/shadow and keeps hit_*; pass B (after shade) does the reverse.
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
    // Enumerated once: `|=` on separate lines silently drifts when a stage is added.
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

    pathStateSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_PATH_STATE);
    shadowStateSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_SHADOW_STATE);
    queueCounters.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_QUEUE_COUNTERS);
    for (size_t q = 0; q < queueIndices.size(); ++q) {
        queueIndices[q].bindBase(GL_SHADER_STORAGE_BUFFER, QUEUE_BINDINGS[q]);
    }
    dispatchArgsSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_DISPATCH_ARGS);
    dispatchArgsSSBO.bind(GL_DISPATCH_INDIRECT_BUFFER);

    constexpr GL::Barrier BARRIER = GL::Barrier::Storage | GL::Barrier::Command | GL::Barrier::BufferUpdate;

    queueCounters.clear();

    generate.use();
    GL::dispatch(targets.numGroupsX, targets.numGroupsY);
    GL::memoryBarrier(BARRIER);

    for (int b = 0; b < maxBounces; ++b) {
        prepareIndirect.use();
        prepareIndirect.setUInt("clear_mask", CLEAR_MASK_PRE_SHADE);
        GL::dispatch(1);
        GL::memoryBarrier(BARRIER);

        // Shade kernels read disjoint hit_* queues and only append to ray/shadow, so one barrier covers all.
        shadeOpaque.use();
        shadeOpaque.setInt("bounce_index", b);
        GL::dispatchIndirect(Q_OPAQUE * DISPATCH_ARG_STRIDE);

        shadeTransmissive.use();
        shadeTransmissive.setInt("bounce_index", b);
        GL::dispatchIndirect(Q_TRANSMISSIVE * DISPATCH_ARG_STRIDE);

        shadeEmissive.use();
        GL::dispatchIndirect(Q_EMISSIVE * DISPATCH_ARG_STRIDE);

        GL::memoryBarrier(BARRIER);

        prepareIndirect.use();
        prepareIndirect.setUInt("clear_mask", CLEAR_MASK_POST_SHADE);
        GL::dispatch(1);
        GL::memoryBarrier(BARRIER);

        traceShadow.use();
        GL::dispatchIndirect(Q_SHADOW * DISPATCH_ARG_STRIDE);
        GL::memoryBarrier(BARRIER);

        // The last bounce has no next hit to route continuation rays into.
        if (b + 1 < maxBounces) {
            trace.use();
            GL::dispatchIndirect(Q_RAY * DISPATCH_ARG_STRIDE);
            GL::memoryBarrier(BARRIER);
        }
    }

    targets.accum.bindForAccumulation();
    targets.normals.bind(2, GL_WRITE_ONLY);
    targets.moments.bind(3, GL_READ_WRITE);

    resolve.use();
    GL::dispatch(targets.numGroupsX, targets.numGroupsY);
    GL::memoryBarrier(GL::Barrier::ImageAccess);
}
