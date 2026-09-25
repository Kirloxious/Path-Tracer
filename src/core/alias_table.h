#pragma once

#include <cstdint>
#include <span>
#include <vector>

/// Vose alias table: draw a slot uniformly, keep it with probability accept[i], else take alias[i].
struct AliasTable
{
    std::vector<float>    accept;
    std::vector<uint32_t> alias;
};

/// Weights must be non-negative; an all-zero set yields a uniform table.
[[nodiscard]] AliasTable buildAliasTable(std::span<const float> weights);
