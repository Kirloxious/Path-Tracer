#pragma once

#include <random>

inline float randomFloat() {
    static std::mt19937                          generator(std::random_device{}()); // internal state but no manual seed
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}