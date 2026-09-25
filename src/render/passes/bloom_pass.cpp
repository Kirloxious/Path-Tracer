#include "render/passes/bloom_pass.h"

#include <algorithm>

#include "core/log.h"
#include "gpu/gl.h"

namespace {
constexpr int MIP_COUNT = 5;
constexpr int MIN_MIP_DIM = 4; // avoids degenerate filters
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
        // Must match the `layout(rgba16f)` in bloom_*.comp: the final upsample writes targets.hdr, and
        // a mismatched format class makes imageLoad/imageStore undefined.
        mips.emplace_back(mw, mh, GL_RGBA16F);
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

    downsampleShader.use();
    downsampleShader.setFloat("threshold", settings.bloomThreshold);
    downsampleShader.setFloat("knee", settings.bloomKnee);

    for (int i = 0; i < static_cast<int>(mips.size()); ++i) {
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

    upsampleShader.use();
    upsampleShader.setFloat("radius", settings.bloomFilterRadius);

    for (int i = static_cast<int>(mips.size()) - 1; i > 0; --i) {
        mips[i].bindSampler(0);
        // In-place additive blend.
        mips[i - 1].bind(1, GL_READ_WRITE);
        upsampleShader.setIVec2("dst_size", mips[i - 1].width, mips[i - 1].height);
        upsampleShader.setFloat("strength", 1.0f);

        const int gx = (mips[i - 1].width + 7) / 8;
        const int gy = (mips[i - 1].height + 7) / 8;
        GL::dispatch(gx, gy);
        GL::memoryBarrier(IMG_BARRIER);
    }

    mips[0].bindSampler(0);
    targets.hdr.bind(1, GL_READ_WRITE);
    upsampleShader.setIVec2("dst_size", targets.hdr.width, targets.hdr.height);
    upsampleShader.setFloat("strength", settings.bloomStrength);
    GL::dispatch(targets.numGroupsX, targets.numGroupsY);
    GL::memoryBarrier(IMG_BARRIER);
}
