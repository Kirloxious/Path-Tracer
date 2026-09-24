#pragma once

/**
 * @file render_settings.h
 * @brief Runtime-tunable rendering knobs shared between the GUI and the passes.
 */

#include "core/shader_shared.h"

/**
 * @brief Debug AOV overlay selector.
 *
 * The values are the `AOV_*` defines from host_shared.glsl, which `aov.comp` switches on.
 */
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

/**
 * @brief Runtime-tunable rendering knobs shared between the GUI and the passes that consume them.
 *
 * Owned by Application, edited by the GUI, and handed to passes through RenderContext::settings.
 *
 * Anything added here must be safely mutable *without* resetting `frameIndex`, which would
 * invalidate progressive accumulation. Values that affect the integrand belong in Scene or
 * Camera, not here.
 */
struct RenderSettings
{
    /// Multiplied into the color before tonemap. Slider range [0.05, 5.0].
    /// Read directly by TonemapPass when auto-exposure is disabled.
    float exposure = 0.5f;

    /// AOV overlay: when != None, AovPass overwrites the display texture with a
    /// debug visualization of the selected buffer.
    AovMode aovMode = AovMode::None;

    /// Far endpoint for the linear-depth visualization — depth is normalized against it.
    float aovDepthMax = 20.0f;
    /// Upper endpoint for the BVH-cost heatmap, in traversal steps.
    float aovBvhCostMax = 200.0f;

    // -------- Bloom --------
    bool  bloomEnabled = true;
    float bloomStrength = 0.06f;    ///< Final additive gain on the composite.
    float bloomThreshold = 1.0f;    ///< Luminance above which bloom starts (soft-knee).
    float bloomKnee = 0.5f;         ///< Width of the soft knee around the threshold.
    float bloomFilterRadius = 1.0f; ///< 3x3 tent radius in destination texels for the upsample.

    // -------- Auto exposure --------
    bool autoExposureEnabled = true;
    /// Lower end of the log2-luminance histogram range. -8 log2 ≈ 1/256 — together with
    /// autoExposureLogMax this is a comfortable range for a mixed indoor/outdoor path-traced scene.
    float autoExposureLogMin = -8.0f;
    /// Upper end of the log2-luminance histogram range. +4 log2 ≈ 16.
    float autoExposureLogMax = 4.0f;
    float autoExposureTau = 1.0f;         ///< EMA time constant in seconds (larger = slower adaptation).
    float autoExposureTargetLuma = 0.18f; ///< "Middle gray" target — standard photography convention.
    float autoExposureMin = 0.05f;        ///< Clamp floor on the computed exposure.
    float autoExposureMax = 8.0f;         ///< Clamp ceiling on the computed exposure.
};
