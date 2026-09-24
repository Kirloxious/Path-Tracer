#pragma once

/**
 * @file alias_table.h
 * @brief Vose alias tables: O(1) sampling from a discrete distribution.
 */

#include <cstdint>
#include <span>
#include <vector>

/**
 * @brief A slot is drawn uniformly, kept with probability `accept[i]`, otherwise replaced by
 *        `alias[i]` — one load and one compare per draw, against the log2(n) chain of
 *        dependent loads a CDF binary search costs on the GPU.
 */
struct AliasTable
{
    std::vector<float>    accept;
    std::vector<uint32_t> alias;
};

/**
 * @brief Builds the alias table for drawing slot i with probability weights[i] / sum(weights).
 * @param weights Non-negative weights. If they sum to zero the table is uniform.
 */
[[nodiscard]] AliasTable buildAliasTable(std::span<const float> weights);
