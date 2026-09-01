#pragma once

/**
 * @file utils.h
 * @brief Small CPU-side helpers with no better home.
 */

#include <random>

/**
 * @brief Returns a uniformly distributed float in [0, 1).
 *
 * Backed by a function-local `static` Mersenne Twister seeded once from `std::random_device`,
 * so successive calls advance a shared stream and different runs produce different sequences.
 * CPU-side only — GPU sampling uses the per-path RNG in `shader/common/rng.glsl`.
 *
 * @return A random float in [0, 1).
 * @note Not thread-safe: the generator is shared mutable state.
 */
inline float randomFloat() {
    static std::mt19937                          generator(std::random_device{}()); // internal state but no manual seed
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}
