#pragma once

#include <frame_buffer.h>
#include <texture.h>

#include "gbuffer.h"

struct RenderTargets
{
    static constexpr int WORK_GROUP_SIZE = 8;

    Texture accum;         // path tracer output — single-frame noisy sample
    Texture normals;       // primary normals + material type, consumed by the denoiser
    Texture denoised_ping; // A-Trous ping-pong
    Texture display;       // final image blitted to the swap chain

    // gbuf is the current frame's primary-visibility data; gbuf_prev holds the
    // previous frame's, used for temporal reprojection (ReSTIR temporal reuse,
    // motion-vector validation). RasterGBufferPass swaps the pair every frame
    // so downstream code can always read `gbuf` as "current" / `gbuf_prev` as
    // "last frame".
    GBuffer gbuf;
    GBuffer gbuf_prev;

    FrameBuffer fb; // wraps `display`, used for the final swap-chain blit

    GLuint numGroupsX = 0;
    GLuint numGroupsY = 0;

    RenderTargets(int w, int h);
    void resize(int w, int h);
};
