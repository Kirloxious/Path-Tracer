#pragma once

#include <atomic>
#include <vector>
#include <glm/glm.hpp>

#include "primitive.h"

struct AABB
{
    glm::vec3 min{};
    glm::vec3 max{};

    [[nodiscard]] glm::vec3 center() const { return (min + max) * 0.5f; }

    AABB& expand(float delta) {
        float padding = delta * 0.5f;
        min -= padding;
        max += padding;
        return *this;
    }

    [[nodiscard]] float surfaceArea() const {
        glm::vec3 d = max - min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    AABB& pad() {
        constexpr float delta = 0.0001f;
        for (int i = 0; i < 3; ++i) {
            if (max[i] - min[i] < delta) {
                min[i] -= delta;
                max[i] += delta;
            }
        }
        return *this;
    }
};

[[nodiscard]] AABB surroundingBox(const AABB& a, const AABB& b);
[[nodiscard]] AABB computeAABB(const Triangle& t, const std::vector<Vertex>& vertices);

struct alignas(16) BVHNodeFlat
{
    glm::vec4  aabbMin; // .xyz = min
    glm::vec4  aabbMax; // .xyz = max
    glm::ivec4 meta;    // interior: .x = left, .y = right, .z = -1,         .w = skip
                        // leaf:     .x = -1,   .y = 0,     .z = triangleIdx, .w = skip
};

class BVH
{
public:
    std::vector<BVHNodeFlat> nodes;
    int                      root = -1;

    void build(const std::vector<Triangle>& triangles, const std::vector<Vertex>& vertices);

private:
    struct Node
    {
        AABB aabb{};
        int  left = -1;
        int  right = -1;
        int  primitiveIndex = -1;
        int  subtreeSize = 1;

        [[nodiscard]] bool isLeaf() const { return primitiveIndex != -1; }
    };

    static constexpr int NUM_BINS = 16;

    struct Bin
    {
        AABB aabb{};
        int  count = 0;
    };

    [[nodiscard]] static int buildR(std::vector<Node>& tree, std::atomic<int>& nextSlot, const std::vector<AABB>& aabbs,
                                    const std::vector<glm::vec3>& centroids, int* begin, int* end);

    [[nodiscard]] int flatten(int nodeIndex, const std::vector<Node>& tree, const std::vector<int>& triangleIndices, int nextAfterSubtree);
};
