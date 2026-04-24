#include "bvh.h"
#include "log.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <future>
#include <numeric>

namespace {
// Minimum primitive count at which buildR parallelizes its two recursive calls.
// Above this, left subtree is spawned on std::async, right runs on the current thread.
// Chosen so only the top few levels parallelize; deeper levels stay serial to amortize
// thread creation cost.
constexpr int PAR_THRESHOLD = 16384;
} // namespace

AABB computeAABB(const Sphere& s) {
    glm::vec3 rvec(s.radius);
    return {s.position - rvec, s.position + rvec};
}

AABB computeAABB(const Triangle& t) {
    glm::vec3 v1 = t.v0 + t.e1;
    glm::vec3 v2 = t.v0 + t.e2;
    AABB      aabb{};
    aabb.min = glm::min(glm::min(t.v0, v1), v2);
    aabb.max = glm::max(glm::max(t.v0, v1), v2);
    aabb.pad();
    return aabb;
}

AABB computeAABB(const Quad& q) {
    AABB aabb{};
    aabb = {q.corner_point, q.corner_point + q.u + q.v};
    aabb.pad();
    return aabb;
}

AABB surroundingBox(const AABB& a, const AABB& b) {
    return {glm::min(a.min, b.min), glm::max(a.max, b.max)};
}

int BVH::buildR(std::vector<Node>& tree, std::atomic<int>& nextSlot, const std::vector<AABB>& aabbs, const std::vector<glm::vec3>& centroids,
                int* begin, int* end) {
    assert(begin < end);

    Node node;

    node.aabb = aabbs[*begin];
    for (int* p = begin + 1; p < end; ++p) {
        node.aabb = surroundingBox(node.aabb, aabbs[*p]);
    }

    const int n = static_cast<int>(end - begin);

    // Leaf — single primitive
    if (n == 1) {
        node.primitiveIndex = *begin;
        node.subtreeSize = 1;
        int idx = nextSlot.fetch_add(1, std::memory_order_relaxed);
        tree[idx] = std::move(node);
        return idx;
    }

    // Small node — midpoint split on longest axis
    if (n <= 4) {
        glm::vec3 extent = node.aabb.max - node.aabb.min;
        int       axis = (extent.y > extent.x) ? ((extent.z > extent.y) ? 2 : 1) : ((extent.z > extent.x) ? 2 : 0);
        int       mid = n / 2;
        std::nth_element(begin, begin + mid, end, [&](int a, int b) { return centroids[a][axis] < centroids[b][axis]; });
        node.left = buildR(tree, nextSlot, aabbs, centroids, begin, begin + mid);
        node.right = buildR(tree, nextSlot, aabbs, centroids, begin + mid, end);
        node.subtreeSize = 1 + tree[node.left].subtreeSize + tree[node.right].subtreeSize;
        int idx = nextSlot.fetch_add(1, std::memory_order_relaxed);
        tree[idx] = std::move(node);
        return idx;
    }

    // Binned SAH
    float bestCost = FLT_MAX;
    int   bestAxis = 0;
    int   bestBinSplit = 1;

    AABB centroidBounds{};
    centroidBounds.min = centroids[*begin];
    centroidBounds.max = centroidBounds.min;
    for (int* p = begin + 1; p < end; ++p) {
        glm::vec3 c = centroids[*p];
        centroidBounds.min = glm::min(centroidBounds.min, c);
        centroidBounds.max = glm::max(centroidBounds.max, c);
    }

    glm::vec3 centroidExtent = centroidBounds.max - centroidBounds.min;

    // All centroids coincide — no axis carries spatial information. Split the range
    // in its current order to keep recursion bounded.
    if (centroidExtent.x < 1e-6f && centroidExtent.y < 1e-6f && centroidExtent.z < 1e-6f) {
        int* mid = begin + n / 2;
        node.left = buildR(tree, nextSlot, aabbs, centroids, begin, mid);
        node.right = buildR(tree, nextSlot, aabbs, centroids, mid, end);
        node.subtreeSize = 1 + tree[node.left].subtreeSize + tree[node.right].subtreeSize;
        int idx = nextSlot.fetch_add(1, std::memory_order_relaxed);
        tree[idx] = std::move(node);
        return idx;
    }

    const float     invParentArea = 1.0f / std::max(node.aabb.surfaceArea(), 1e-12f);
    constexpr float T_TRAV = 0.125f;

    for (int axis = 0; axis < 3; ++axis) {
        if (centroidExtent[axis] < 1e-6f) {
            continue;
        }

        Bin   bins[NUM_BINS] = {};
        float scale = static_cast<float>(NUM_BINS) / centroidExtent[axis];

        for (int* p = begin; p < end; ++p) {
            int b = static_cast<int>((centroids[*p][axis] - centroidBounds.min[axis]) * scale);
            b = std::clamp(b, 0, NUM_BINS - 1);
            bins[b].count++;
            if (bins[b].count == 1) {
                bins[b].aabb = aabbs[*p];
            } else {
                bins[b].aabb = surroundingBox(bins[b].aabb, aabbs[*p]);
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
            float rightArea = sweepBox.surfaceArea();
            float cost = T_TRAV + (leftAreas[i] * static_cast<float>(leftCounts[i]) + rightArea * static_cast<float>(sweepCount)) * invParentArea;
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

    float splitScale = static_cast<float>(NUM_BINS) / centroidExtent[bestAxis];
    int*  mid = std::partition(begin, end, [&](int idx) {
        int b = static_cast<int>((centroids[idx][bestAxis] - centroidBounds.min[bestAxis]) * splitScale);
        b = std::clamp(b, 0, NUM_BINS - 1);
        return b < bestBinSplit;
    });

    if (mid == begin || mid == end) {
        mid = begin + n / 2;
        std::nth_element(begin, mid, end, [&](int a, int b) { return centroids[a][bestAxis] < centroids[b][bestAxis]; });
    }

    int leftIdx;
    int rightIdx;
    if (n > PAR_THRESHOLD) {
        auto leftFuture = std::async(std::launch::async, [&] { return buildR(tree, nextSlot, aabbs, centroids, begin, mid); });
        rightIdx = buildR(tree, nextSlot, aabbs, centroids, mid, end);
        leftIdx = leftFuture.get();
    } else {
        leftIdx = buildR(tree, nextSlot, aabbs, centroids, begin, mid);
        rightIdx = buildR(tree, nextSlot, aabbs, centroids, mid, end);
    }
    node.left = leftIdx;
    node.right = rightIdx;
    node.subtreeSize = 1 + tree[leftIdx].subtreeSize + tree[rightIdx].subtreeSize;

    int idx = nextSlot.fetch_add(1, std::memory_order_relaxed);
    tree[idx] = std::move(node);
    return idx;
}

void BVH::build(const std::vector<Sphere>& spheres, const std::vector<Triangle>& triangles) {
    assert(!spheres.empty() || !triangles.empty());

    std::vector<AABB>         aabbs;
    std::vector<glm::vec3>    centroids;
    std::vector<PrimitiveRef> primRefs;

    const auto numSpheres = spheres.size();
    const auto numTriangles = triangles.size();
    const auto totalPrims = numSpheres + numTriangles;

    aabbs.reserve(totalPrims);
    centroids.reserve(totalPrims);
    primRefs.reserve(totalPrims);

    for (int i = 0; i < static_cast<int>(numSpheres); ++i) {
        aabbs.push_back(computeAABB(spheres[i]));
        centroids.push_back(aabbs.back().center());
        primRefs.push_back({0, i});
    }
    for (int i = 0; i < static_cast<int>(numTriangles); ++i) {
        aabbs.push_back(computeAABB(triangles[i]));
        centroids.push_back(aabbs.back().center());
        primRefs.push_back({1, i});
    }

    Log::info("Building BVH over {} spheres + {} triangles", numSpheres, numTriangles);

    const int        n = static_cast<int>(totalPrims);
    std::vector<int> indices(static_cast<std::size_t>(n));
    std::iota(indices.begin(), indices.end(), 0);

    // Binary tree with n single-prim leaves has exactly 2n - 1 nodes. Pre-size so
    // concurrent buildR calls can claim slots via nextSlot without reallocation.
    std::vector<Node> tree(static_cast<std::size_t>(2 * n - 1));
    std::atomic<int>  nextSlot{0};

    const int treeRoot = buildR(tree, nextSlot, aabbs, centroids, indices.data(), indices.data() + n);

    nodes.clear();
    nodes.reserve(static_cast<std::size_t>(tree[treeRoot].subtreeSize));
    root = flatten(treeRoot, tree, primRefs, -1);

    Log::info("BVH: {} nodes", nodes.size());
}

int BVH::flatten(int nodeIndex, const std::vector<Node>& tree, const std::vector<PrimitiveRef>& primRefs, int nextAfterSubtree) {
    if (nodeIndex < 0) {
        return nextAfterSubtree;
    }

    const Node& node = tree[nodeIndex];
    int         currentIndex = static_cast<int>(nodes.size());
    nodes.emplace_back();

    // Leaf node — meta: x=-1, y=primType, z=primIndex, w=skip
    if (node.isLeaf()) {
        const auto& ref = primRefs[node.primitiveIndex];
        nodes[currentIndex] = {glm::vec4(node.aabb.min, 0.0f), glm::vec4(node.aabb.max, 0.0f), glm::ivec4(-1, ref.type, ref.index, nextAfterSubtree)};
        return currentIndex;
    }

    // Interior — lay out [self, left-subtree, right-subtree] so near-child cache locality
    // follows parent. Left-subtree size is known from the build phase.
    const int leftSubtreeSize = tree[node.left].subtreeSize;
    const int rightStart = currentIndex + 1 + leftSubtreeSize;

    int leftFlat = flatten(node.left, tree, primRefs, rightStart);
    int rightFlat = flatten(node.right, tree, primRefs, nextAfterSubtree);

    assert(leftFlat == currentIndex + 1);
    assert(rightFlat == rightStart);

    nodes[currentIndex] = {glm::vec4(node.aabb.min, 0.0f), glm::vec4(node.aabb.max, 0.0f), glm::ivec4(leftFlat, rightFlat, -1, nextAfterSubtree)};

    return currentIndex;
}
