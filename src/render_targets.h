#pragma once

#include <frame_buffer.h>
#include <texture.h>

#include "gbuffer.h"

struct RenderTargets
{
    static constexpr int WORK_GROUP_SIZE = 8;

    Texture accum;
    Texture normals;
    Texture denoised_ping;
    Texture display;

    GBuffer gbuf;

    FrameBuffer fb; // wraps `display`, used for the final swap-chain blit

    GLuint numGroupsX = 0;
    GLuint numGroupsY = 0;

    RenderTargets(int w, int h);
    void resize(int w, int h);
};
