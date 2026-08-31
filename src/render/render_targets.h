#pragma once

#include <gpu/frame_buffer.h>
#include <gpu/texture.h>

#include "gpu/gbuffer.h"

class EnvMap;

struct RenderTargets
{
    static constexpr int WORK_GROUP_SIZE = 8;

    Texture accum;         // path tracer output — single-frame noisy sample
    Texture normals;       // primary normals + material type, consumed by the denoiser
    Texture denoised_ping; // A-Trous ping-pong
    Texture hdr;           // HDR pre-tonemap image — output of denoiser, modified by bloom, read by auto-exposure + tonemap
    Texture display;       // final LDR image blitted to the swap chain
    Texture taa_history;   // previous frame's TAA-resolved LDR image (sampled bilinear for reprojection)
    Texture taa_output;    // scratch target for this frame's TAA write; copied into display and swapped into taa_history

    // gbuf is the current frame's primary-visibility data; gbuf_prev holds the
    // previous frame's, used for temporal reprojection (ReSTIR temporal reuse,
    // motion-vector validation). RasterGBufferPass swaps the pair every frame
    // so downstream code can always read `gbuf` as "current" / `gbuf_prev` as
    // "last frame".
    GBuffer gbuf;
    GBuffer gbuf_prev;

    FrameBuffer fb; // wraps `display`, used for the final swap-chain blit

    // Optional per-scene HDR envmap. Owned by Renderer; Renderer sets this pointer
    // in loadScene() (null if the scene has no envmap). Passes read it in execute().
    const EnvMap* envMap = nullptr;

    GLuint numGroupsX = 0;
    GLuint numGroupsY = 0;

    RenderTargets(int w, int h);
    void resize(int w, int h);
};
