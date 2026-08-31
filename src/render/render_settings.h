#pragma once

// Runtime-tunable rendering knobs shared between the GUI and the passes that
// consume them. Shared by reference from Application → GuiPass and to any
// pass that needs a value (see denoiser_pass.h for the pattern).
//
// Anything added here must be safely mutable *without* resetting frameIndex
// (which invalidates progressive accumulation). Values that affect the
// integrand belong in Scene/Camera, not here.

// Values must stay in sync with shader/aov.comp AOV_* constants.
enum class AovMode : int
{
    None = 0,
    WorldNormal = 1,
    LinearDepth = 2,
    Albedo = 3,
    MaterialId = 4,
    BvhCost = 5,
    Variance = 6,
};

struct RenderSettings
{
    // Multiplied into the color before tonemap. Slider range [0.05, 5.0].
    // Read directly by TonemapPass when auto-exposure is disabled.
    float exposure = 0.5f;

    // AOV overlay: when != None, AovPass overwrites the display texture with a
    // debug visualization of the selected buffer.
    AovMode aovMode = AovMode::None;

    // Normalization endpoints for the depth and BVH-cost visualizations.
    float aovDepthMax = 20.0f;
    float aovBvhCostMax = 200.0f;

    // -------- Bloom --------
    bool  bloomEnabled = true;
    float bloomStrength = 0.06f;    // final additive gain on the composite
    float bloomThreshold = 1.0f;    // luminance above which bloom starts (soft-knee)
    float bloomKnee = 0.5f;         // width of the soft knee around threshold
    float bloomFilterRadius = 1.0f; // 3x3 tent radius in destination texels for upsample

    // -------- Auto exposure --------
    bool autoExposureEnabled = true;
    // Log2-luminance histogram range. -8 log2 ≈ 1/256, +4 log2 ≈ 16 — comfortable
    // range for a mixed indoor/outdoor path-traced scene.
    float autoExposureLogMin = -8.0f;
    float autoExposureLogMax = 4.0f;
    float autoExposureTau = 1.0f;         // EMA time constant in seconds (larger = slower adaptation)
    float autoExposureTargetLuma = 0.18f; // "middle gray" target — standard photography convention
    float autoExposureMin = 0.05f;
    float autoExposureMax = 8.0f;
};
