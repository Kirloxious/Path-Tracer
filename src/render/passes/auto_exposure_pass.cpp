#include "render/passes/auto_exposure_pass.h"

#include <array>
#include <algorithm>

#include "core/log.h"

namespace {
constexpr GLuint BIND_EXPOSURE = 30;
constexpr GLuint BIND_HISTOGRAM = 31;
constexpr int    HIST_BINS = 256;
} // namespace

AutoExposurePass::AutoExposurePass(int w, int h, const RenderSettings& s) : width(w), height(h), settings(s) {
    Log::info("AutoExposurePass: loading histogram/reduce shaders");
    histogramShader = ComputeShader("shader/luminance_histogram.comp");
    reduceShader = ComputeShader("shader/auto_exposure.comp");

    // ExposureBuffer: { float exposure; float pad[3]; }. Seed with the user's
    // exposure so the first frame doesn't display a random gain.
    const std::array<float, 4> init = {s.exposure, 0.0f, 0.0f, 0.0f};
    exposureSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_EXPOSURE, init.data(), sizeof(init), GL_DYNAMIC_COPY);

    // HistogramBuffer: 256 uint bins, zero-initialised.
    const std::array<GLuint, HIST_BINS> zeros{};
    histogramSSBO = Buffer(GL_SHADER_STORAGE_BUFFER, BIND_HISTOGRAM, zeros.data(), sizeof(zeros), GL_DYNAMIC_COPY);
}

void AutoExposurePass::resize(int w, int h) {
    width = w;
    height = h;
    // Re-seed EMA on the new pixel count so the first post-resize frame doesn't
    // blend a stale exposure derived from a histogram of a different image size.
    primed = false;
}

bool AutoExposurePass::reloadIfChanged(const RenderContext&) {
    bool any = false;
    any |= histogramShader.reloadIfChanged();
    any |= reduceShader.reloadIfChanged();
    return any;
}

void AutoExposurePass::execute(const RenderContext& ctx, RenderTargets& targets) {
    // Keep the SSBO bound each frame — a scene switch (or nothing else touching
    // these bindings) may have knocked the base-binding association off between
    // frames.
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_EXPOSURE, exposureSSBO.id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_HISTOGRAM, histogramSSBO.id);

    if (!settings.autoExposureEnabled) {
        // Manual exposure: just write the slider value into the SSBO so tonemap
        // sees the same field either way.
        exposureSSBO.update(settings.exposure);
        primed = false; // re-seed EMA next time auto-exposure is re-enabled
        return;
    }

    const float logLumaMin = settings.autoExposureLogMin;
    const float logLumaMax = settings.autoExposureLogMax;
    const float logLumaRange = logLumaMax - logLumaMin;
    const float invLogLumaRange = (logLumaRange > 1e-4f) ? 1.0f / logLumaRange : 1.0f;

    // ---- Histogram pass ----
    histogramShader.use();
    targets.hdr.bind(0, GL_READ_ONLY);
    histogramShader.setIVec2("image_size", width, height);
    histogramShader.setFloat("log_luma_min", logLumaMin);
    histogramShader.setFloat("inv_log_luma_range", invLogLumaRange);

    // Each workgroup is 16x16 threads and each thread walks ROWS_PER_THREAD rows, so a
    // group covers 16 columns x (16 * ROWS_PER_THREAD) rows. Must match the constant in
    // luminance_histogram.comp — too few groups silently drops the bottom of the image
    // from the histogram, which shows up as exposure drifting on dark floors.
    constexpr int HIST_ROWS_PER_THREAD = 8;
    constexpr int HIST_ROWS_PER_GROUP = 16 * HIST_ROWS_PER_THREAD;
    const int     gx = (width + 15) / 16;
    const int     gy = (height + HIST_ROWS_PER_GROUP - 1) / HIST_ROWS_PER_GROUP;
    glDispatchCompute(gx, gy, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ---- Reduce + EMA + zero-histogram ----
    reduceShader.use();
    reduceShader.setFloat("log_luma_min", logLumaMin);
    reduceShader.setFloat("log_luma_range", logLumaRange);
    // Cap dt so a debugger pause doesn't blow the EMA past the target next frame.
    const float dt = std::clamp(ctx.dt, 1e-4f, 0.1f);
    reduceShader.setFloat("dt", dt);
    reduceShader.setFloat("tau", settings.autoExposureTau);
    reduceShader.setFloat("target_luma", settings.autoExposureTargetLuma);
    reduceShader.setFloat("min_exposure", settings.autoExposureMin);
    reduceShader.setFloat("max_exposure", settings.autoExposureMax);
    reduceShader.setInt("reset_exposure", primed ? 0 : 1);
    primed = true;

    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}
