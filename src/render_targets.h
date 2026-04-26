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

    GBuffer gbuf;

    FrameBuffer fb; // wraps `display`, used for the final swap-chain blit

    GLuint numGroupsX = 0;
    GLuint numGroupsY = 0;

    RenderTargets(int w, int h);
    void resize(int w, int h);
};
