#pragma once

#include <random>

/// Uniform float in [0, 1). Not thread-safe: the generator is shared static state.
inline float randomFloat() {
    static std::mt19937                          generator(std::random_device{}());
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}
