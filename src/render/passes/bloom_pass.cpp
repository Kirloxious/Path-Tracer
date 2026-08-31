#include "render/passes/bloom_pass.h"

#include <algorithm>

#include "core/log.h"

namespace {
constexpr int MIP_COUNT = 5;
constexpr int MIN_MIP_DIM = 4; // stop halving when smaller than this to avoid degenerate filters
} // namespace

BloomPass::BloomPass(int w, int h, const RenderSettings& s) : settings(s) {
    Log::info("BloomPass: loading downsample/upsample shaders");
    downsampleShader = ComputeShader("shader/bloom_downsample.comp");
    upsampleShader = ComputeShader("shader/bloom_upsample.comp");
    buildMips(w, h);
}

void BloomPass::buildMips(int w, int h) {
    mips.clear();
    mipWidths.clear();
    mipHeights.clear();

    int mw = w;
    int mh = h;
    mips.reserve(MIP_COUNT);
    mipWidths.reserve(MIP_COUNT);
    mipHeights.reserve(MIP_COUNT);
    for (int i = 0; i < MIP_COUNT; ++i) {
        mw = std::max(mw / 2, MIN_MIP_DIM);
        mh = std::max(mh / 2, MIN_MIP_DIM);
        // Must match the `layout(rgba32f, ...)` in bloom_{down,up}sample.comp — the
        // upsample shader's final dispatch writes into targets.hdr (rgba32f), so
        // all intermediate mips need to be in the same image-format compatibility
        // class (rgba16f ≠ rgba32f per Table 8.27), otherwise imageLoad/imageStore
        // return undefined values (visible as a magenta blob elsewhere in the frame).
        mips.emplace_back(mw, mh, GL_RGBA32F);
        // Bilinear so the 13-tap downsample and 3x3 tent upsample can sample
        // between texels without hand-rolling weights per corner.
        glTextureParameteri(mips.back().handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mips.back().handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        mipWidths.push_back(mw);
        mipHeights.push_back(mh);
    }
    Log::info("BloomPass: mip chain {}x{} → {}x{}", mipWidths.front(), mipHeights.front(), mipWidths.back(), mipHeights.back());
}

void BloomPass::resize(int w, int h) {
    buildMips(w, h);
}

bool BloomPass::reloadIfChanged(const RenderContext&) {
    bool any = false;
    any |= downsampleShader.reloadIfChanged();
    any |= upsampleShader.reloadIfChanged();
    return any;
}

void BloomPass::execute(const RenderContext&, RenderTargets& targets) {
    if (!settings.bloomEnabled) {
        return;
    }

    constexpr GLbitfield IMG_BARRIER = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT;

    // -------- Downsample chain: hdr → mip[0] → mip[1] → ... → mip[n-1] --------
    downsampleShader.use();
    downsampleShader.setFloat("threshold", settings.bloomThreshold);
    downsampleShader.setFloat("knee", settings.bloomKnee);

    for (int i = 0; i < static_cast<int>(mips.size()); ++i) {
        // First pass reads the HDR image and applies the soft-knee prefilter;
        // subsequent passes chain mip[i-1] → mip[i] with the raw downsample.
        GLuint srcHandle = (i == 0) ? targets.hdr.handle : mips[i - 1].handle;
        glBindTextureUnit(0, srcHandle);
        mips[i].bind(1, GL_WRITE_ONLY);

        downsampleShader.setIVec2("dst_size", mipWidths[i], mipHeights[i]);
        downsampleShader.setInt("apply_threshold", i == 0 ? 1 : 0);

        int gx = (mipWidths[i] + 7) / 8;
        int gy = (mipHeights[i] + 7) / 8;
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(IMG_BARRIER);
    }

    // -------- Upsample chain: mip[n-1] additively → mip[n-2] → ... → mip[0] --------
    upsampleShader.use();
    upsampleShader.setFloat("radius", settings.bloomFilterRadius);

    for (int i = static_cast<int>(mips.size()) - 1; i > 0; --i) {
        glBindTextureUnit(0, mips[i].handle);
        // The upsample shader does an in-place additive blend, so bind rw.
        mips[i - 1].bind(1, GL_READ_WRITE);
        upsampleShader.setIVec2("dst_size", mipWidths[i - 1], mipHeights[i - 1]);
        upsampleShader.setFloat("strength", 1.0f); // intermediate mips: no attenuation

        int gx = (mipWidths[i - 1] + 7) / 8;
        int gy = (mipHeights[i - 1] + 7) / 8;
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(IMG_BARRIER);
    }

    // -------- Final composite: mip[0] additively blended into hdr with `strength` --------
    glBindTextureUnit(0, mips[0].handle);
    targets.hdr.bind(1, GL_READ_WRITE);
    upsampleShader.setIVec2("dst_size", targets.hdr.width, targets.hdr.height);
    upsampleShader.setFloat("strength", settings.bloomStrength);
    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(IMG_BARRIER);
}
