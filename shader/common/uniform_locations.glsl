#ifndef UNIFORM_LOCATIONS_GLSL
#define UNIFORM_LOCATIONS_GLSL

// Single source of truth for every explicit `layout(location = N) uniform`.
//
// Locations are a per-program namespace, but headers are shared across programs,
// so a header-owned uniform must claim the same number everywhere. Two ranges:
//
//   0..7    private to one kernel. Reuse freely — restir_spatial's LOC_PASS_INDEX
//           and denoiser's LOC_STEP_SIZE can both be 0 because no program links both.
//   8..31   owned by a common/ header. Unique across the whole shader tree; a
//           collision here is a link error, which is the point of this file.
//
// Sampler/image `binding` numbers live in a different namespace and are NOT here —
// those are documented in CLAUDE.md's binding table.

// ---- Private range (0..7). Named per kernel for readability only. ----
#define LOC_IMAGE_SIZE      0
#define LOC_FRAME_INDEX     1
#define LOC_PARAM_2         2
#define LOC_PARAM_3         3
#define LOC_PARAM_4         4
#define LOC_PARAM_5         5
#define LOC_PARAM_6         6
#define LOC_PARAM_7         7

// ---- Header-owned range (8..31). Unique tree-wide. ----
#define LOC_BVH_ROOT_INDEX      8   // common/bvh_traversal.glsl
#define LOC_EMISSIVE_LAST_INDEX 9   // reserved: common/bvh_traversal.glsl emissive-prefix shadow test
#define LOC_INDIRECT_CLAMP      10  // common/clamp.glsl
#define LOC_NUM_LIGHT_GROUPS    11  // shade_lambertian.comp, shade_emissive.comp
#define LOC_BOUNCE_INDEX        12  // shade_* kernels (shared shape, so pinned here)
#define LOC_MAX_BOUNCES         13  // shade_* kernels
#define LOC_ENV_MAP_VALID       20  // common/envmap.glsl
#define LOC_ENV_MAP_INTENSITY   21  // common/envmap.glsl

#endif
