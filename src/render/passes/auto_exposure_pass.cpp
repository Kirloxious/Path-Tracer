#include "render/passes/auto_exposure_pass.h"

#include <array>
#include <algorithm>

#include "core/log.h"
#include "core/shader_shared.h"
#include "gpu/gl.h"

namespace {
constexpr int HIST_BINS = 256;
} // namespace

AutoExposurePass::AutoExposurePass(float initialExposure)
    : histogramShader("shader/luminance_histogram.comp"), reduceShader("shader/auto_exposure.comp") {

    // Seeded with the user's exposure so the first frame doesn't display a random gain.
    const std::array<float, 4> init = {initialExposure, 0.0f, 0.0f, 0.0f};
    exposureSSBO = Buffer(init, GL_DYNAMIC_COPY);

    const std::array<GLuint, HIST_BINS> zeros{};
    histogramSSBO = Buffer(zeros, GL_DYNAMIC_COPY);
}

void AutoExposurePass::resize(int, int) {
    primed = false;
}

bool AutoExposurePass::reloadIfChanged() {
    bool any = false;
    any |= histogramShader.reloadIfChanged();
    any |= reduceShader.reloadIfChanged();
    return any;
}

void AutoExposurePass::execute(const RenderContext& ctx, RenderTargets& targets) {
    const RenderSettings& settings = ctx.settings;
    exposureSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_EXPOSURE);
    histogramSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_HISTOGRAM);

    if (!settings.autoExposureEnabled) {
        // Written anyway so tonemap reads one field either way.
        exposureSSBO.update(settings.exposure);
        primed = false; // re-seed the EMA on re-enable
        return;
    }

    const float logLumaMin = settings.autoExposureLogMin;
    const float logLumaMax = settings.autoExposureLogMax;
    const float logLumaRange = logLumaMax - logLumaMin;
    const float invLogLumaRange = (logLumaRange > 1e-4f) ? 1.0f / logLumaRange : 1.0f;

    histogramShader.use();
    targets.hdr.bind(0, GL_READ_ONLY);
    histogramShader.setFloat("log_luma_min", logLumaMin);
    histogramShader.setFloat("inv_log_luma_range", invLogLumaRange);

    // Each group covers 16 columns x (16 * ROWS_PER_THREAD) rows; must match luminance_histogram.comp,
    // or the bottom of the image silently drops out of the histogram.
    constexpr int HIST_ROWS_PER_THREAD = 8;
    constexpr int HIST_ROWS_PER_GROUP = 16 * HIST_ROWS_PER_THREAD;
    const int     gx = (targets.width + 15) / 16;
    const int     gy = (targets.height + HIST_ROWS_PER_GROUP - 1) / HIST_ROWS_PER_GROUP;
    GL::dispatch(gx, gy);
    GL::memoryBarrier(GL::Barrier::Storage);

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
    reduceShader.setFloat("low_percentile", settings.autoExposureLowPercentile);
    reduceShader.setFloat("high_percentile", std::max(settings.autoExposureHighPercentile, settings.autoExposureLowPercentile));
    reduceShader.setInt("reset_exposure", primed ? 0 : 1);
    primed = true;

    GL::dispatch(1);
    GL::memoryBarrier(GL::Barrier::Storage);
}
