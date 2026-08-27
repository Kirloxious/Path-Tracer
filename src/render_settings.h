#pragma once

// Runtime-tunable rendering knobs shared between the GUI and the passes that
// consume them. Shared by reference from Application → GuiPass and to any
// pass that needs a value (see denoiser_pass.h for the pattern).
//
// Anything added here must be safely mutable *without* resetting frameIndex
// (which invalidates progressive accumulation). Values that affect the
// integrand belong in Scene/Camera, not here.
struct RenderSettings
{
    // Multiplied into the color before tonemap. Slider range [0.05, 5.0].
    float exposure = 0.5f;
};
