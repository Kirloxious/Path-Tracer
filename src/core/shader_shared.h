#pragma once

/**
 * @file shader_shared.h
 * @brief Binding points, texture units, queue slots and enum values shared with the shaders.
 *
 * The definitions live in shader/common/host_shared.glsl, which is valid for both the C and
 * GLSL preprocessors, so the host and the shaders read one list instead of mirroring it.
 */

#include "common/host_shared.glsl"
