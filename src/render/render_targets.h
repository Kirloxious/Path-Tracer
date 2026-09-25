#pragma once

#include <gpu/frame_buffer.h>
#include <gpu/texture.h>

#include "gpu/gbuffer.h"

struct RenderTargets
{
    static constexpr int WORK_GROUP_SIZE = 8;

    Texture accum;   ///< Running-average radiance; `.a` is the sample count.
    Texture moments; ///< Running luminance moments (E[l], E[l^2]).
    Texture normals;
    Texture denoised_ping;
    Texture hdr;
    Texture tonemapped; ///< Separate from `display` so TAA can read neighbours while writing.
    Texture display;
    Texture taa_history;
    Texture taa_output;

    GBuffer gbuf;
    /// Nothing reads it today; kept rotated for future temporal consumers.
    GBuffer gbuf_prev;

    FrameBuffer fb; ///< Wraps `display` for the swap-chain blit.

    int width = 0;
    int height = 0;

    GLuint numGroupsX = 0;
    GLuint numGroupsY = 0;

    RenderTargets(int w, int h);

    /// All contents are lost; the caller must reset `frameIndex`.
    void resize(int w, int h);

    /// Called by Renderer before the first pass.
    void beginFrame();

    /// Called by Renderer after the last pass.
    void endFrame();

private:
    void allocate(int w, int h);
};
