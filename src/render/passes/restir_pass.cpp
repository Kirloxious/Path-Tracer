#include "render/passes/restir_pass.h"

#include <array>
#include <functional>

#include "core/log.h"
#include "core/shader_shared.h"

namespace {
constexpr int M_INITIAL_DEFAULT = 32;
// Cap on prev.M during temporal combine. ~20× M_initial is the standard
// Bitterli/Wyman recommendation: enough history to converge on static frames,
// short enough that lighting changes don't stick around for many frames.
constexpr int   M_CAP_DEFAULT = 20 * M_INITIAL_DEFAULT;
constexpr int   SPATIAL_K = 5;
constexpr float SPATIAL_RADIUS_PASS_1 = 30.0f;
constexpr float SPATIAL_RADIUS_PASS_2 = 15.0f;
} // namespace

RestirPass::RestirPass(int w, int h)
    : initial("shader/restir_initial.comp"), temporal("shader/restir_temporal.comp"), spatial("shader/restir_spatial.comp") {
    allocate(w, h);
}

void RestirPass::allocate(int w, int h) {
    const size_t numPixels = static_cast<size_t>(w) * static_cast<size_t>(h);
    Log::info("RestirPass: {}x{} = {} pixels, reservoir SSBO = {} bytes (×2 for ping-pong)", w, h, numPixels, numPixels * sizeof(ReservoirData));

    reservoirsA = Buffer(nullptr, numPixels * sizeof(ReservoirData), GL_DYNAMIC_COPY);
    reservoirsB = Buffer(nullptr, numPixels * sizeof(ReservoirData), GL_DYNAMIC_COPY);
    surfacesA = Buffer(nullptr, numPixels * sizeof(RestirSurfaceData), GL_DYNAMIC_COPY);
    surfacesB = Buffer(nullptr, numPixels * sizeof(RestirSurfaceData), GL_DYNAMIC_COPY);
    clearHistory();
    useAAsCurrent = true;
}

void RestirPass::clearHistory() {
    for (const Buffer* b : {&reservoirsA, &reservoirsB, &surfacesA, &surfacesB}) {
        b->clear();
    }
}

void RestirPass::onSceneLoaded(const Scene&) {
    clearHistory();
}

void RestirPass::resize(int w, int h) {
    allocate(w, h);
}

bool RestirPass::reloadIfChanged() {
    bool any = false;
    for (ComputeShader& kernel : std::array<std::reference_wrapper<ComputeShader>, 3>{initial, temporal, spatial}) {
        any |= kernel.reloadIfChanged();
    }
    if (any) {
        clearHistory();
    }
    return any;
}

void RestirPass::execute(const RenderContext&, RenderTargets& targets) {
    // This frame writes into whichever buffer held frame N-2; frame N-1 becomes "prev".
    const Buffer& current = useAAsCurrent ? reservoirsA : reservoirsB;
    const Buffer& prev = useAAsCurrent ? reservoirsB : reservoirsA;
    current.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_CURRENT);
    prev.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_PREV);

    // Unlike the reservoirs, the spatial passes never repurpose the prev surfaces as scratch,
    // so these two bindings stay put for the whole pass.
    (useAAsCurrent ? surfacesA : surfacesB).bindBase(GL_SHADER_STORAGE_BUFFER, BIND_SURFACES_CURRENT);
    (useAAsCurrent ? surfacesB : surfacesA).bindBase(GL_SHADER_STORAGE_BUFFER, BIND_SURFACES_PREV);

    initial.use();
    initial.setInt("m_initial", M_INITIAL_DEFAULT);
    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    temporal.use();
    temporal.setInt("m_cap", M_CAP_DEFAULT);
    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Spatial reuse: two ping-ponged passes with shrinking radius. The shader reads
    // `spatial_input` (binding 20) and writes `reservoirs` (binding 18). The prev buffer is
    // scratch for pass 1 — next frame's initial pass overwrites it anyway — and pass 2
    // lands the final result back in `current` for shade_opaque to consume.
    spatial.use();
    spatial.setInt("k_neighbors", SPATIAL_K);

    prev.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_CURRENT);
    current.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_SPATIAL_INPUT);
    spatial.setFloat("radius_pixels", SPATIAL_RADIUS_PASS_1);
    spatial.setInt("pass_index", 0);
    // Pass 2 re-validates whatever sample survives, so this pass skips its shadow ray.
    spatial.setInt("test_visibility", 0);
    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    current.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_CURRENT);
    prev.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_RESERVOIRS_SPATIAL_INPUT);
    spatial.setFloat("radius_pixels", SPATIAL_RADIUS_PASS_2);
    spatial.setInt("pass_index", 1);
    spatial.setInt("test_visibility", 1);
    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    useAAsCurrent = !useAAsCurrent;
}
