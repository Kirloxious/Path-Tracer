#pragma once

#include "core/shader_shared.h"

/// Values are the `AOV_*` defines from host_shared.glsl.
enum class AovMode : int
{
    None = AOV_NONE,
    WorldNormal = AOV_WORLD_NORMAL,
    LinearDepth = AOV_LINEAR_DEPTH,
    Albedo = AOV_ALBEDO,
    MaterialId = AOV_MATERIAL_ID,
    BvhCost = AOV_BVH_COST,
    Variance = AOV_VARIANCE,
};

/// Everything here must be safely mutable without resetting `frameIndex`; anything affecting the
/// integrand belongs in Scene or Camera.
struct RenderSettings
{
    /// Used by TonemapPass when auto-exposure is disabled.
    float exposure = 0.5f;

    AovMode aovMode = AovMode::None;

    float aovDepthMax = 20.0f;
    /// In traversal steps.
    float aovBvhCostMax = 200.0f;

    // -------- Bloom --------
    bool  bloomEnabled = true;
    float bloomStrength = 0.06f;
    float bloomThreshold = 1.0f;
    float bloomKnee = 0.5f;
    float bloomFilterRadius = 1.0f; ///< In destination texels.

    // -------- Auto exposure --------
    bool  autoExposureEnabled = true;
    float autoExposureLogMin = -8.0f;
    float autoExposureLogMax = 4.0f;
    float autoExposureTau = 1.0f; ///< EMA time constant in seconds.
    float autoExposureTargetLuma = 0.18f;
    float autoExposureMin = 0.05f;
    float autoExposureMax = 8.0f;
    /// Fractions of non-black pixels dropped from each end of the histogram, so a small emitter or
    /// deep shadow doesn't swing the exposure of everything else.
    float autoExposureLowPercentile = 0.05f;
    float autoExposureHighPercentile = 0.95f;
};
