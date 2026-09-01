#include "scene/bvh.h"
#include "core/log.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <future>
#include <limits>
#include <bit>
#include <numeric>
#include <span>

namespace {
// Minimum primitive count at which buildR parallelizes its two recursive calls.
// Above this, left subtree is spawned on std::async, right runs on the current thread.
// Chosen so only the top few levels parallelize; deeper levels stay serial to amortize
// thread creation cost.
constexpr int PAR_THRESHOLD = 16384;
} // namespace

AABB computeAABB(const Triangle& t, const std::vector<Vertex>& vertices) {
    const glm::vec3& p0 = vertices[t.indices.x].position;
    const glm::vec3& p1 = vertices[t.indices.y].position;
    const glm::vec3& p2 = vertices[t.indices.z].position;
    AABB             aabb{};
    aabb.min = glm::min(glm::min(p0, p1), p2);
    aabb.max = glm::max(glm::max(p0, p1), p2);
    aabb.pad();
    return aabb;
}

AABB surroundingBox(const AABB& a, const AABB& b) {
    return {glm::min(a.min, b.min), glm::max(a.max, b.max)};
}

int BVH::buildR(std::vector<Node>& tree, std::atomic<int>& nextSlot, const std::vector<AABB>& aabbs, const int* refsBase,
                const std::vector<glm::vec3>& centroids, std::span<int> range) {
    assert(!range.empty());

    Node node;

    node.aabb = aabbs[range.front()];
    for (const int idx : range.subspan(1)) {
        node.aabb = surroundingBox(node.aabb, aabbs[idx]);
    }

    const size_t n = range.size();

    // Leaf. `range` is a contiguous slice of the shared index array, and buildR only ever
    // permutes within its own slice, so the offset of that slice is a stable run of
    // triangle references the flattened leaf can point at.
    if (n <= static_cast<size_t>(MAX_LEAF_TRIANGLES)) {
        node.firstRef = static_cast<int>(range.data() - refsBase);
        node.refCount = static_cast<int>(n);
        node.subtreeSize = 1;
        const int slot = nextSlot.fetch_add(1, std::memory_order_relaxed);
        tree[slot] = std::move(node);
        return slot;
    }

    // Claim a slot for `node` after recursing into both halves, wiring up the child links
    // and subtree size. Shared by all three split strategies below.
    auto emitInterior = [&](int leftIdx, int rightIdx) {
        node.left = leftIdx;
        node.right = rightIdx;
        node.subtreeSize = 1 + tree[leftIdx].subtreeSize + tree[rightIdx].subtreeSize;
        const int slot = nextSlot.fetch_add(1, std::memory_order_relaxed);
        tree[slot] = std::move(node);
        return slot;
    };

    auto buildSerial = [&](size_t mid) {
        const int leftIdx = buildR(tree, nextSlot, aabbs, refsBase, centroids, range.subspan(0, mid));
        const int rightIdx = buildR(tree, nextSlot, aabbs, refsBase, centroids, range.subspan(mid));
        return emitInterior(leftIdx, rightIdx);
    };

    // Binned SAH
    float bestCost = std::numeric_limits<float>::infinity();
    int   bestAxis = 0;
    int   bestBinSplit = 1;

    AABB centroidBounds{};
    centroidBounds.min = centroids[range.front()];
    centroidBounds.max = centroidBounds.min;
    for (const int idx : range.subspan(1)) {
        const glm::vec3 c = centroids[idx];
        centroidBounds.min = glm::min(centroidBounds.min, c);
        centroidBounds.max = glm::max(centroidBounds.max, c);
    }

    const glm::vec3 centroidExtent = centroidBounds.max - centroidBounds.min;

    // All centroids coincide — no axis carries spatial information. Split the range
    // in its current order to keep recursion bounded.
    if (centroidExtent.x < 1e-6f && centroidExtent.y < 1e-6f && centroidExtent.z < 1e-6f) {
        return buildSerial(n / 2);
    }

    const float     invParentArea = 1.0f / std::max(node.aabb.surfaceArea(), 1e-12f);
    constexpr float T_TRAV = 0.125f;

    for (int axis = 0; axis < 3; ++axis) {
        if (centroidExtent[axis] < 1e-6f) {
            continue;
        }

        Bin         bins[NUM_BINS] = {};
        const float scale = static_cast<float>(NUM_BINS) / centroidExtent[axis];

        for (const int idx : range) {
            int b = static_cast<int>((centroids[idx][axis] - centroidBounds.min[axis]) * scale);
            b = std::clamp(b, 0, NUM_BINS - 1);
            bins[b].count++;
            if (bins[b].count == 1) {
                bins[b].aabb = aabbs[idx];
            } else {
                bins[b].aabb = surroundingBox(bins[b].aabb, aabbs[idx]);
            }
        }

        float leftAreas[NUM_BINS - 1];
        int   leftCounts[NUM_BINS - 1];
        AABB  sweepBox = bins[0].aabb;
        int   sweepCount = bins[0].count;
        for (int i = 1; i < NUM_BINS; ++i) {
            leftAreas[i - 1] = (sweepCount > 0) ? sweepBox.surfaceArea() : 0.0f;
            leftCounts[i - 1] = sweepCount;
            if (bins[i].count > 0) {
                sweepBox = (sweepCount > 0) ? surroundingBox(sweepBox, bins[i].aabb) : bins[i].aabb;
                sweepCount += bins[i].count;
            }
        }

        sweepBox = bins[NUM_BINS - 1].aabb;
        sweepCount = bins[NUM_BINS - 1].count;
        for (int i = NUM_BINS - 2; i >= 0; --i) {
            if (leftCounts[i] == 0 || sweepCount == 0) {
                if (bins[i].count > 0) {
                    sweepBox = (sweepCount > 0) ? surroundingBox(sweepBox, bins[i].aabb) : bins[i].aabb;
                    sweepCount += bins[i].count;
                }
                continue;
            }
            const float rightArea = sweepBox.surfaceArea();
            const float cost =
                T_TRAV + (leftAreas[i] * static_cast<float>(leftCounts[i]) + rightArea * static_cast<float>(sweepCount)) * invParentArea;
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestBinSplit = i + 1;
            }
            if (bins[i].count > 0) {
                sweepBox = surroundingBox(sweepBox, bins[i].aabb);
                sweepCount += bins[i].count;
            }
        }
    }

    const float splitScale = static_cast<float>(NUM_BINS) / centroidExtent[bestAxis];
    const auto  midIt = std::partition(range.begin(), range.end(), [&](int idx) {
        int b = static_cast<int>((centroids[idx][bestAxis] - centroidBounds.min[bestAxis]) * splitScale);
        b = std::clamp(b, 0, NUM_BINS - 1);
        return b < bestBinSplit;
    });

    size_t mid = static_cast<size_t>(std::distance(range.begin(), midIt));
    if (mid == 0 || mid == n) {
        mid = n / 2;
        std::nth_element(
            range.begin(), range.begin() + mid, range.end(), [&](int a, int b) { return centroids[a][bestAxis] < centroids[b][bestAxis]; });
    }

    if (n > PAR_THRESHOLD) {
        auto leftFuture = std::async(std::launch::async, [&] { return buildR(tree, nextSlot, aabbs, refsBase, centroids, range.subspan(0, mid)); });
        const int rightIdx = buildR(tree, nextSlot, aabbs, refsBase, centroids, range.subspan(mid));
        return emitInterior(leftFuture.get(), rightIdx);
    }
    return buildSerial(mid);
}

void BVH::build(const std::vector<Triangle>& triangles, const std::vector<Vertex>& vertices) {
    assert(!triangles.empty());

    const int n = static_cast<int>(triangles.size());

    std::vector<AABB>      aabbs;
    std::vector<glm::vec3> centroids;
    aabbs.reserve(n);
    centroids.reserve(n);

    for (int i = 0; i < n; ++i) {
        aabbs.push_back(computeAABB(triangles[i], vertices));
        centroids.push_back(aabbs.back().center());
    }

    Log::info("Building BVH over {} triangles", n);

    // buildR permutes this in place; a leaf's `primitiveIndex` is already the triangle
    // index, so flatten needs no second identity mapping.
    std::vector<int> indices(static_cast<std::size_t>(n));
    std::iota(indices.begin(), indices.end(), 0);

    // Binary tree with n single-prim leaves has exactly 2n - 1 nodes. Pre-size so
    // concurrent buildR calls can claim slots via nextSlot without reallocation.
    std::vector<Node> tree(static_cast<std::size_t>(2 * n - 1));
    std::atomic<int>  nextSlot{0};

    const int treeRoot = buildR(tree, nextSlot, aabbs, indices.data(), centroids, indices);

    nodes.clear();
    nodes.reserve(static_cast<std::size_t>(tree[treeRoot].subtreeSize));
    maxDepth = 0;
    root = flatten(treeRoot, tree, 1);

    // buildR permuted `indices` in place; a leaf's (firstRef, refCount) addresses a run in
    // it, so the finished permutation *is* the reference array the GPU needs.
    triRefs = std::move(indices);

    Log::info("BVH: {} nodes, {} triangle refs, max depth {}", nodes.size(), triRefs.size(), maxDepth);
}

int BVH::flatten(int nodeIndex, const std::vector<Node>& tree, int depth) {
    const Node& node = tree[nodeIndex];
    const int   currentIndex = static_cast<int>(nodes.size());
    nodes.emplace_back();

    maxDepth = std::max(maxDepth, depth);

    // Leaf — aabbMin.w carries the first triangle ref, aabbMax.w the count (> 0).
    if (node.isLeaf()) {
        nodes[currentIndex] = {glm::vec4(node.aabb.min, std::bit_cast<float>(node.firstRef)),
                               glm::vec4(node.aabb.max, std::bit_cast<float>(node.refCount))};
        return currentIndex;
    }

    // Interior — lay out [self, left subtree, right subtree]. The left child is therefore
    // always currentIndex + 1 and needs no storage; only the right child index is written,
    // into aabbMin.w. aabbMax.w stays 0, which is what marks the node as interior.
    const int leftFlat = flatten(node.left, tree, depth + 1);
    const int rightFlat = flatten(node.right, tree, depth + 1);

    assert(leftFlat == currentIndex + 1);
    (void)leftFlat;

    nodes[currentIndex] = {glm::vec4(node.aabb.min, std::bit_cast<float>(rightFlat)), glm::vec4(node.aabb.max, std::bit_cast<float>(0))};

    return currentIndex;
}
