#pragma once

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
        constexpr float delta = 0.001f;
        if (surfaceArea() < delta)
            expand(delta);
        return *this;
    }
};

[[nodiscard]] AABB surroundingBox(const AABB& a, const AABB& b);
[[nodiscard]] AABB computeAABB(const Sphere& s);
[[nodiscard]] AABB computeAABB(const Quad& q);

struct alignas(16) BVHNodeFlat
{
    glm::vec4  aabbMin; // .xyz = min
    glm::vec4  aabbMax; // .xyz = max
    glm::ivec4 meta;    // .x = left, .y = right, .z = primitiveIndex, .w = nextAfterSubtree
};

class BVH
{
public:
    std::vector<BVHNodeFlat> nodes;
    int                      root = -1;

    void build(const std::vector<Sphere>& spheres);

private:
    struct Node
    {
        AABB aabb{};
        int  left = -1;
        int  right = -1;
        int  primitiveIndex = -1;

        [[nodiscard]] bool isLeaf() const { return primitiveIndex != -1; }
    };

    struct SplitResult
    {
        int              splitIndex = 0;
        std::vector<int> sorted;
    };

    [[nodiscard]] static float computeSAHCost(float numLeft, float leftArea, float numRight, float rightArea);

    [[nodiscard]] static SplitResult
    findBestSplit(const std::vector<AABB>& aabbs, const std::vector<int>& indices, std::vector<AABB>& scratchLeft, std::vector<AABB>& scratchRight);

    [[nodiscard]] static int buildR(std::vector<Node>&       tree,
                                    const std::vector<AABB>& aabbs,
                                    std::vector<int>         indices,
                                    std::vector<AABB>&       scratchLeft,
                                    std::vector<AABB>&       scratchRight);

    [[nodiscard]] int flatten(int nodeIndex, const std::vector<Node>& tree, int nextAfterSubtree);
};
