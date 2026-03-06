#include "bvh.h"
#include "log.h"
#include <cassert>
#include <numeric>

AABB computeAABB(const Sphere& s) {
    glm::vec3 rvec(s.radius);
    return {s.position - rvec, s.position + rvec};
}

AABB computeAABB(const Quad& q) {
    AABB aabb{};
    aabb = {q.corner_point, q.corner_point + q.u + q.v};
    aabb.pad(); // in case quad is exactly on bouding box, add some padding
    return aabb;
}

AABB surroundingBox(const AABB& a, const AABB& b) {
    return {glm::min(a.min, b.min), glm::max(a.max, b.max)};
}

float BVH::computeSAHCost(float numLeft, float leftArea, float numRight, float rightArea) {
    // traversal cost = 1.0, intersection cost = 1.0
    return 1.0f + leftArea * numLeft + rightArea * numRight;
}

// SAH reduces intersection tests by splitting the AABB at the position that minimises
// the weighted sum of child surface areas (surface area heuristic).
BVH::SplitResult
BVH::findBestSplit(const std::vector<AABB>& aabbs, const std::vector<int>& indices, std::vector<AABB>& scratchLeft, std::vector<AABB>& scratchRight) {
    const int n = static_cast<int>(indices.size());

    float bestCost = FLT_MAX;
    int   bestAxis = 0;
    int   bestSplitIdx = 1;

    std::vector<int> sorted(indices); // one allocation, reused per axis via std::sort

    for (int axis = 0; axis < 3; ++axis) {
        std::sort(sorted.begin(), sorted.end(), [&](int a, int b) { return aabbs[a].center()[axis] < aabbs[b].center()[axis]; });

        // prefix sweep: scratchLeft[i] = bounding box of sorted[0..i]
        scratchLeft[0] = aabbs[sorted[0]];
        for (int i = 1; i < n; ++i)
            scratchLeft[i] = surroundingBox(scratchLeft[i - 1], aabbs[sorted[i]]);

        // suffix sweep: scratchRight[i] = bounding box of sorted[i..n-1]
        scratchRight[n - 1] = aabbs[sorted[n - 1]];
        for (int i = n - 2; i >= 0; --i)
            scratchRight[i] = surroundingBox(scratchRight[i + 1], aabbs[sorted[i]]);

        for (int i = 1; i < n; ++i) {
            float cost =
                computeSAHCost(static_cast<float>(i), scratchLeft[i - 1].surfaceArea(), static_cast<float>(n - i), scratchRight[i].surfaceArea());
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestSplitIdx = i;
            }
        }
    }

    // Sort once along the winning axis, then return the result
    std::sort(sorted.begin(), sorted.end(), [&](int a, int b) { return aabbs[a].center()[bestAxis] < aabbs[b].center()[bestAxis]; });

    return SplitResult{bestSplitIdx, std::move(sorted)};
}

int BVH::buildR(std::vector<Node>&       tree,
                const std::vector<AABB>& aabbs,
                std::vector<int>         indices,
                std::vector<AABB>&       scratchLeft,
                std::vector<AABB>&       scratchRight) {
    assert(!indices.empty());

    Node node;

    // Compute bounding box over all primitives in this node
    node.aabb = aabbs[indices[0]];
    for (int i = 1; i < static_cast<int>(indices.size()); ++i)
        node.aabb = surroundingBox(node.aabb, aabbs[indices[i]]);

    // Leaf — single primitive
    if (indices.size() == 1u) {
        node.primitiveIndex = indices[0];
        tree.push_back(std::move(node));
        return static_cast<int>(tree.size()) - 1;
    }

    // Interior — find best SAH split and recurse
    SplitResult split = findBestSplit(aabbs, indices, scratchLeft, scratchRight);

    std::vector<int> leftIndices(split.sorted.begin(), split.sorted.begin() + split.splitIndex);
    std::vector<int> rightIndices(split.sorted.begin() + split.splitIndex, split.sorted.end());

    node.left = buildR(tree, aabbs, std::move(leftIndices), scratchLeft, scratchRight);
    node.right = buildR(tree, aabbs, std::move(rightIndices), scratchLeft, scratchRight);

    tree.push_back(std::move(node));
    return static_cast<int>(tree.size()) - 1;
}

void BVH::build(const std::vector<Sphere>& spheres) {
    assert(!spheres.empty());

    std::vector<AABB> aabbs;
    aabbs.reserve(spheres.size());
    for (const auto& s : spheres)
        aabbs.push_back(computeAABB(s));

    Log::info("Building BVH over {} spheres", spheres.size());

    const int        n = static_cast<int>(spheres.size());
    std::vector<int> indices(static_cast<std::size_t>(n));
    std::iota(indices.begin(), indices.end(), 0);

    std::vector<Node> tree;
    tree.reserve(static_cast<std::size_t>(2 * n));

    std::vector<AABB> scratchLeft(static_cast<std::size_t>(n));
    std::vector<AABB> scratchRight(static_cast<std::size_t>(n));

    const int treeRoot = buildR(tree, aabbs, std::move(indices), scratchLeft, scratchRight);

    nodes.clear();
    nodes.reserve(tree.size());
    root = flatten(treeRoot, tree, -1);
}

int BVH::flatten(int nodeIndex, const std::vector<Node>& tree, int nextAfterSubtree) {
    if (nodeIndex < 0)
        return nextAfterSubtree;

    const Node& node = tree[nodeIndex];
    int         currentIndex = static_cast<int>(nodes.size());
    nodes.emplace_back();

    // Leaf node
    if (node.isLeaf()) {
        nodes[currentIndex] = {
            glm::vec4(node.aabb.min, 0.0f), glm::vec4(node.aabb.max, 0.0f), glm::ivec4(-1, -1, node.primitiveIndex, nextAfterSubtree)};
        return currentIndex;
    }

    // Internal node
    int rightFlat = flatten(node.right, tree, nextAfterSubtree);
    int leftFlat = flatten(node.left, tree, rightFlat);

    nodes[currentIndex] = {glm::vec4(node.aabb.min, 0.0f), glm::vec4(node.aabb.max, 0.0f), glm::ivec4(leftFlat, rightFlat, -1, nextAfterSubtree)};

    return currentIndex;
}
