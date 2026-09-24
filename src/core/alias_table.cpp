#include "core/alias_table.h"

#include <algorithm>

AliasTable buildAliasTable(std::span<const float> weights) {
    const std::size_t n = weights.size();
    AliasTable        table{std::vector<float>(n, 1.0f), std::vector<uint32_t>(n, 0)};

    double total = 0.0;
    for (const float w : weights) {
        total += std::max(w, 0.0f);
    }

    // Scaled so the mean is exactly 1: every slot is either under- or over-full, and each
    // under-full one can be topped up from a single over-full one.
    std::vector<float>    p(n);
    std::vector<uint32_t> small, large;
    small.reserve(n);
    large.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        p[i] = total > 0.0 ? static_cast<float>(static_cast<double>(n) * std::max(weights[i], 0.0f) / total) : 1.0f;
        (p[i] < 1.0f ? small : large).push_back(i);
    }

    while (!small.empty() && !large.empty()) {
        const uint32_t l = small.back();
        small.pop_back();
        const uint32_t g = large.back();
        large.pop_back();

        table.accept[l] = p[l];
        table.alias[l] = g;
        p[g] = (p[g] + p[l]) - 1.0f;
        (p[g] < 1.0f ? small : large).push_back(g);
    }
    // Whatever remains is full to within rounding and keeps its accept of 1.
    return table;
}
