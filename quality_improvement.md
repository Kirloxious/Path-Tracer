# Render Quality Improvement Plan

## Context

ReSTIR DI is now fully implemented (initial + temporal + spatial reuse, hooked into `shade_lambertian.comp` for primary direct lighting). With direct-lighting variance largely tamed for Lambertian primaries, the dominant remaining noise sources are:

1. **Indirect specular caustics / fireflies** — high-throughput paths through dielectrics or off rough metals can produce single-sample radiance spikes that the denoiser smears rather than resolves.
2. **Material variety is limited** — metals are perfect-specular with a fuzz hack; dielectrics are ideal Fresnel; no roughness sampling.
3. **Tonemap is a hand-rolled Reinhard** that desaturates highlights.

This document sequences improvements from highest ROI / lowest cost to highest cost. Each item is independently shippable.

---

## Improvement 1 — Firefly clamping / path regularization

**Why first:** ~10 lines, no architectural change, immediate visible win on the Cornell-with-glass-sphere case. ReSTIR doesn't help indirect caustics — this does.

**Files to modify**
- `shader/shade_emissive.comp` — clamp the per-bounce contribution before accumulating into `s.radiance` at line 61.
- `shader/shade_lambertian.comp` — clamp the ReSTIR contribution at line 55 and the inline-NEE contribution at line 103 the same way.
- `src/path_tracer_pass.cpp` — add a uniform `firefly_max` (default 10.0) and a `regularize_after_bounce` int (default 1).
- `src/gui.cpp` — expose both as sliders.

**Steps**
1. In `shade_emissive.comp`, compute `vec3 contrib = s.throughput * mat.color * mat.emission * weight;` then `contrib = min(contrib, vec3(firefly_max));` before `s.radiance += contrib`.
2. In `shade_lambertian.comp` apply the same clamp to the ReSTIR-direct contribution and to `s.nee_le` before queueing the shadow ray.
3. **Path regularization (optional second sub-step):** in `shade_metal.comp` and `shade_dielectric.comp`, after `s.bounce >= regularize_after_bounce`, widen the lobe — for metal, force `fuzz = max(fuzz, 0.1)`; for dielectric, jitter the reflected/refracted direction slightly. This converts spike caustics into bandlimited glow that the denoiser can resolve. Gate behind a uniform so it's toggleable from the GUI.
4. Add `firefly_max` (1.0–100.0 slider, default 10.0) and `regularize_after_bounce` (0–4 slider, default 1) to the GUI.

**Verification**
- Switch to `Showcase()` scene, accumulate ~512 spp. Before: bright pinholes near the dielectric sphere. After: bounded, denoise-friendly highlights.
- Visually compare with clamping disabled — image should be nearly identical except for the brightest pixels.

---

## Improvement 2 — Expose ReSTIR tunables in the GUI

**Why now:** Cheap and unblocks tuning during the rest of this work. Currently `M_INITIAL`, `M_CAP`, `SPATIAL_K`, and the two radii are compile-time constants in `src/restir_pass.cpp:10–17`.

**Files to modify**
- `src/restir_pass.h` — promote the five constants to instance fields with the existing defaults.
- `src/restir_pass.cpp` — read instance fields in `uploadUniforms()` and `execute()` instead of the namespace constants.
- `src/gui.cpp` / `src/gui_pass.{h,cpp}` — add a "ReSTIR" collapsing header with sliders. Hold a `RestirPass*` reference.
- `src/application.cpp` — pass the `RestirPass` pointer to the gui pass at construction.

**Steps**
1. Move constants from anonymous namespace into `RestirPass` as public members.
2. Add ImGui sliders: `M_INITIAL` (1–128), `M_CAP` (M_INITIAL–2048), `SPATIAL_K` (0–8), `SPATIAL_RADIUS_PASS_1` (1–64), `SPATIAL_RADIUS_PASS_2` (1–32). Setting `SPATIAL_K=0` skips spatial passes entirely (useful as an A/B toggle).
3. Add a "Reset reservoirs" button that calls `clearReservoirBuffers()` — needed when the user changes M_CAP and wants old history flushed.

**Verification**
- Slide `M_INITIAL` from 1 to 64 on the `Showcase()` scene and watch the noise floor drop.
- Set `SPATIAL_K=0`, observe spatial-reuse-off baseline; restore.

---

## Improvement 3 — ACES tonemap

**Why next:** Trivial change that fixes the desaturated highlights baked into the current Reinhard.

**Files to modify**
- `shader/denoiser.comp` — replace lines 68–72.

**Steps**
1. Replace the Reinhard block with the Narkowicz ACES fit:
   ```glsl
   const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
   result = clamp((result*(a*result+b))/(result*(c*result+d)+e), 0.0, 1.0);
   ```
2. Keep the `if (apply_tonemapping == 1)` gate.
3. The existing linear→sRGB step (lines 17–19) still runs after tonemap — leave it alone.

**Verification**
- Run `CornellBox()` and compare highlight saturation on the area light vs. the ceiling. ACES preserves chroma in highlights where Reinhard washes to white.
- Confirm dark regions don't crush — ACES toe is gentler than Reinhard's, so blacks should remain readable.

---

## Improvement 4 — GGX rough metals + MIS

**Why now:** Material variety. Currently `Material::Metal` has only `color` and `fuzz` (perfect specular + jitter); we'd add `roughness` and a real microfacet sampler. This is the first improvement that needs CPU-side struct changes.

**Files to modify**
- `src/material.h` — add `float roughness` to `Material` (it has padding room — confirm `alignas(16)` invariants and update any `static_assert(sizeof)`). Add a `Material::RoughMetal(color, roughness)` factory.
- `shader/common/scene_buffers.glsl` — mirror the new field on the GLSL `Material` struct.
- `shader/shade_metal.comp` — replace the `reflected + fuzz*random_unit_vector` block with a GGX visible-normals sampler (Heitz 2018). Compute the BSDF and pdf properly.
- `shader/shade_metal.comp` — enable NEE on rough metals (skip on `roughness < 0.05`, fall through to the existing perfect-specular branch). Set `FLAG_PREV_NON_SPECULAR` on rough hits so emissive MIS works on the next bounce.
- `src/scene.cpp` — convert at least one metal in `Showcase()` to a rough metal to validate.

**Steps**
1. Add `roughness` field; static-assert the struct size; update GLSL struct.
2. Implement `sample_ggx_vndf(N, V, alpha, rng)` and a Smith G2 helper in a new `shader/common/ggx.glsl`.
3. In `shade_metal.comp`: branch on `roughness < 0.05` → existing perfect-specular path (kept for chrome-like surfaces). Else: sample VNDF, compute reflected dir, write `s.pdf_bsdf = pdf_solid_angle`, `s.throughput *= F * G2_over_G1` (the visible-normals form lets you avoid the explicit Jacobian).
4. Add an inline NEE block to the rough branch, MIS-weighted with the BSDF pdf — mirror the structure in `shade_lambertian.comp` lines 65–112.
5. Add a rough copper or brushed-gold sphere to `Showcase()`.

**Verification**
- Render a brushed-metal sphere under the area light. Highlight should be a soft anisotropic-looking glow, not a Dirac dot.
- Compare against the perfect-specular branch by setting `roughness=0.001` — should be visually identical to the old metal.

---

## Improvement 5 — ReSTIR GI (indirect bounces)

**Why last:** Biggest win for indirect-light variance, but also the largest implementation effort. Defer until Improvements 1–4 land and we have a clear baseline.

**Scope sketch (not full design — that gets its own plan):**
- Reservoirs over 2-bounce path suffixes: store `(x1, w1, Le_at_x2)` per pixel.
- Reuses the existing `RestirPass` infrastructure: same buffer ping-pong, same gbuffer reprojection, same temporal/spatial passes.
- Hooks into `shade_lambertian.comp`'s continuation step rather than its NEE step.
- Requires writing the post-first-bounce path state into a sample buffer the spatial/temporal passes can read — extends `path_state.glsl`.

**Decision point:** revisit after Improvements 1–4 ship. If indirect noise is still the bottleneck, plan it out separately. If firefly clamping + ACES + denoiser tuning has gotten the image to "good enough," skip.

---

## Sequencing summary

| # | Improvement | Cost | ROI | Risk |
|---|-------------|------|-----|------|
| 1 | Firefly clamping + regularization | ~30 min | High (visible) | None |
| 2 | ReSTIR GUI tunables | ~45 min | Medium (dev velocity) | None |
| 3 | ACES tonemap | ~5 min | Medium (highlight quality) | None |
| 4 | GGX rough metals + MIS | ~3 hours | High (material variety) | Low — additive, gated by `roughness < 0.05` fallback |
| 5 | ReSTIR GI | ~1–2 days | Very High | High — own plan |

**Critical files referenced**
- `shader/shade_lambertian.comp:55,103,129`
- `shader/shade_metal.comp:22-24,28,32`
- `shader/shade_dielectric.comp:12-43,56`
- `shader/shade_emissive.comp:61`
- `shader/denoiser.comp:17-19,68-72`
- `src/material.h:31-44`
- `src/restir_pass.{h,cpp}` (constants at `cpp:10-17`)
- `src/scene.{h,cpp}` (factory pattern + `sceneRegistry()` at `cpp:198-204`)
- `src/application.cpp:41-44` (pass order — append point only, do not reorder)
- `src/gui.cpp:90-114` (ImGui sections)

## End-to-end verification

After each improvement:
1. `clang-format -i src/*.h src/*.cpp`
2. `cmake --build --preset debug-linux`
3. `./out/build/debug-linux/main`
4. Switch to `CornellBox()` (regression baseline) and `Showcase()` (variance stress) via the scene picker.
5. Accumulate to ~1024 spp and visually compare against a screenshot taken before the change.
6. Toggle the new feature off (slider/checkbox) and confirm parity with pre-change behavior on the regression scene.
