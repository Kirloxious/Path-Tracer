#pragma once

#include <texture.h>
#include <frame_buffer.h>

struct RenderTargets
{
    static constexpr int WORK_GROUP_SIZE = 8;

    Texture     accum;
    Texture     normals;
    Texture     denoised_ping;
    Texture     display;
    FrameBuffer fb;
    GLuint      numGroupsX = 0;
    GLuint      numGroupsY = 0;

    RenderTargets(int w, int h);
    void resize(int w, int h);
};
