#ifndef HOST_SHARED_GLSL
#define HOST_SHARED_GLSL

// Every number the C++ host and the shaders must agree on. src/core/shader_shared.h includes
// this file verbatim, so it may hold only preprocessor directives and // comments.

// ---- SSBO bindings (GL_SHADER_STORAGE_BUFFER namespace) ----
#define BIND_LIGHT_GROUPS             0
#define BIND_MATERIALS                1
#define BIND_BVH_NODES                3
#define BIND_TRIANGLES                4
#define BIND_VERTICES                 5
#define BIND_PATH_STATE               10
#define BIND_QUEUE_COUNTERS           11
#define BIND_RAY_QUEUE                12
#define BIND_HIT_OPAQUE_QUEUE         13
#define BIND_HIT_TRANSMISSIVE_QUEUE   14
#define BIND_HIT_EMISSIVE_QUEUE       15
#define BIND_SHADOW_QUEUE             16
#define BIND_RESERVOIRS_CURRENT       18
#define BIND_RESERVOIRS_PREV          19
#define BIND_RESERVOIRS_SPATIAL_INPUT 20
#define BIND_SHADOW_STATE             22
#define BIND_DISPATCH_ARGS            23
#define BIND_SURFACES_CURRENT         24
#define BIND_SURFACES_PREV            25
#define BIND_TRI_REFS                 26
#define BIND_ENV_SAMPLES              27
#define BIND_EXPOSURE                 30
#define BIND_HISTOGRAM                31

// ---- UBO bindings (GL_UNIFORM_BUFFER namespace) ----
#define UBO_CAMERA 2
#define UBO_FRAME  28
#define UBO_SCENE  29

// ---- Sampler units shared across passes ----
// Post-process kernels use units 0..4 privately; nothing global may live there.
#define TEX_GBUF_NORMAL 6
#define TEX_TAA_HISTORY 7
#define TEX_ENV_MAP     9
#define TEX_GBUF_DEPTH  10

// ---- Wavefront queue slots in QueueCountersBuffer ----
#define Q_RAY          0u
#define Q_OPAQUE       1u
#define Q_TRANSMISSIVE 2u
#define Q_EMISSIVE     3u
#define Q_SHADOW       4u
#define NUM_QUEUES     5u

// ---- Material::type (MaterialClass) ----
#define MAT_DIFFUSE      0u
#define MAT_SPECULAR     1u
#define MAT_TRANSMISSIVE 2u
#define MAT_EMISSIVE     3u

// ---- AOV debug views (AovMode) ----
#define AOV_NONE         0
#define AOV_WORLD_NORMAL 1
#define AOV_LINEAR_DEPTH 2
#define AOV_ALBEDO       3
#define AOV_MATERIAL_ID  4
#define AOV_BVH_COST     5
#define AOV_VARIANCE     6

#endif
