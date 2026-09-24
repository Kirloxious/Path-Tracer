#include "render/passes/bloom_pass.h"

#include <algorithm>

#include "core/log.h"
#include "gpu/gl.h"

namespace {
constexpr int MIP_COUNT = 5;
constexpr int MIN_MIP_DIM = 4; // stop halving when smaller than this to avoid degenerate filters
} // namespace

BloomPass::BloomPass(int w, int h) : downsampleShader("shader/bloom_downsample.comp"), upsampleShader("shader/bloom_upsample.comp") {
    buildMips(w, h);
}

void BloomPass::buildMips(int w, int h) {
    mips.clear();

    int mw = w;
    int mh = h;
    mips.reserve(MIP_COUNT);
    for (int i = 0; i < MIP_COUNT; ++i) {
        mw = std::max(mw / 2, MIN_MIP_DIM);
        mh = std::max(mh / 2, MIN_MIP_DIM);
        // Must match the `layout(rgba16f, ...)` in bloom_{down,up}sample.comp — the
        // upsample shader's final dispatch writes into targets.hdr, so all intermediate
        // mips need to be in the same image-format compatibility class as it (rgba16f ≠
        // rgba32f per Table 8.27), otherwise imageLoad/imageStore return undefined
        // values (visible as a magenta blob elsewhere in the frame).
        mips.emplace_back(mw, mh, GL_RGBA16F);
        // Bilinear so the 13-tap downsample and 3x3 tent upsample can sample
        // between texels without hand-rolling weights per corner.
        mips.back().setFilter(GL_LINEAR);
    }
    Log::info("BloomPass: mip chain {}x{} → {}x{}", mips.front().width, mips.front().height, mips.back().width, mips.back().height);
}

void BloomPass::resize(int w, int h) {
    buildMips(w, h);
}

bool BloomPass::reloadIfChanged() {
    bool any = false;
    any |= downsampleShader.reloadIfChanged();
    any |= upsampleShader.reloadIfChanged();
    return any;
}

void BloomPass::execute(const RenderContext& ctx, RenderTargets& targets) {
    const RenderSettings& settings = ctx.settings;
    if (!settings.bloomEnabled) {
        return;
    }

    constexpr GL::Barrier IMG_BARRIER = GL::Barrier::ImageAccess | GL::Barrier::TextureFetch;

    // -------- Downsample chain: hdr → mip[0] → mip[1] → ... → mip[n-1] --------
    downsampleShader.use();
    downsampleShader.setFloat("threshold", settings.bloomThreshold);
    downsampleShader.setFloat("knee", settings.bloomKnee);

    for (int i = 0; i < static_cast<int>(mips.size()); ++i) {
        // First pass reads the HDR image and applies the soft-knee prefilter;
        // subsequent passes chain mip[i-1] → mip[i] with the raw downsample.
        const Texture& src = (i == 0) ? targets.hdr : mips[i - 1];
        src.bindSampler(0);
        mips[i].bind(1, GL_WRITE_ONLY);

        downsampleShader.setIVec2("dst_size", mips[i].width, mips[i].height);
        downsampleShader.setInt("apply_threshold", i == 0 ? 1 : 0);

        const int gx = (mips[i].width + 7) / 8;
        const int gy = (mips[i].height + 7) / 8;
        GL::dispatch(gx, gy);
        GL::memoryBarrier(IMG_BARRIER);
    }

    // -------- Upsample chain: mip[n-1] additively → mip[n-2] → ... → mip[0] --------
    upsampleShader.use();
    upsampleShader.setFloat("radius", settings.bloomFilterRadius);

    for (int i = static_cast<int>(mips.size()) - 1; i > 0; --i) {
        mips[i].bindSampler(0);
        // The upsample shader does an in-place additive blend, so bind rw.
        mips[i - 1].bind(1, GL_READ_WRITE);
        upsampleShader.setIVec2("dst_size", mips[i - 1].width, mips[i - 1].height);
        upsampleShader.setFloat("strength", 1.0f); // intermediate mips: no attenuation

        const int gx = (mips[i - 1].width + 7) / 8;
        const int gy = (mips[i - 1].height + 7) / 8;
        GL::dispatch(gx, gy);
        GL::memoryBarrier(IMG_BARRIER);
    }

    // -------- Final composite: mip[0] additively blended into hdr with `strength` --------
    mips[0].bindSampler(0);
    targets.hdr.bind(1, GL_READ_WRITE);
    upsampleShader.setIVec2("dst_size", targets.hdr.width, targets.hdr.height);
    upsampleShader.setFloat("strength", settings.bloomStrength);
    GL::dispatch(targets.numGroupsX, targets.numGroupsY);
    GL::memoryBarrier(IMG_BARRIER);
}
