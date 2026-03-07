#include "bvh.h"
#include "log.h"
#include <algorithm>
#include <cassert>
#include <numeric>

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

int BVH::buildR(std::vector<Node>& tree, const std::vector<AABB>& aabbs, int* begin, int* end) {
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
        tree.push_back(std::move(node));
        return static_cast<int>(tree.size()) - 1;
    }

    // Small node — midpoint split on longest axis
    if (n <= 4) {
        glm::vec3 extent = node.aabb.max - node.aabb.min;
        int       axis = (extent.y > extent.x) ? ((extent.z > extent.y) ? 2 : 1) : ((extent.z > extent.x) ? 2 : 0);
        std::sort(begin, end, [&](int a, int b) { return aabbs[a].center()[axis] < aabbs[b].center()[axis]; });
        int mid = n / 2;
        node.left = buildR(tree, aabbs, begin, begin + mid);
        node.right = buildR(tree, aabbs, begin + mid, end);
        tree.push_back(std::move(node));
        return static_cast<int>(tree.size()) - 1;
    }

    // Binned SAH
    float bestCost = FLT_MAX;
    int   bestAxis = 0;
    int   bestBinSplit = 1;

    AABB centroidBounds{};
    centroidBounds.min = aabbs[*begin].center();
    centroidBounds.max = centroidBounds.min;
    for (int* p = begin + 1; p < end; ++p) {
        glm::vec3 c = aabbs[*p].center();
        centroidBounds.min = glm::min(centroidBounds.min, c);
        centroidBounds.max = glm::max(centroidBounds.max, c);
    }

    glm::vec3 centroidExtent = centroidBounds.max - centroidBounds.min;

    for (int axis = 0; axis < 3; ++axis) {
        if (centroidExtent[axis] < 1e-6f) {
            continue;
        }

        Bin   bins[NUM_BINS] = {};
        float scale = static_cast<float>(NUM_BINS) / centroidExtent[axis];

        for (int* p = begin; p < end; ++p) {
            int b = static_cast<int>((aabbs[*p].center()[axis] - centroidBounds.min[axis]) * scale);
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
            float cost = 1.0f + leftAreas[i] * static_cast<float>(leftCounts[i]) + rightArea * static_cast<float>(sweepCount);
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
        int b = static_cast<int>((aabbs[idx].center()[bestAxis] - centroidBounds.min[bestAxis]) * splitScale);
        b = std::clamp(b, 0, NUM_BINS - 1);
        return b < bestBinSplit;
    });

    if (mid == begin || mid == end) {
        mid = begin + n / 2;
    }

    node.left = buildR(tree, aabbs, begin, mid);
    node.right = buildR(tree, aabbs, mid, end);

    tree.push_back(std::move(node));
    return static_cast<int>(tree.size()) - 1;
}

void BVH::build(const std::vector<Sphere>& spheres, const std::vector<Triangle>& triangles) {
    assert(!spheres.empty() || !triangles.empty());

    std::vector<AABB>         aabbs;
    std::vector<PrimitiveRef> primRefs;

    const auto numSpheres = spheres.size();
    const auto numTriangles = triangles.size();
    const auto totalPrims = numSpheres + numTriangles;

    aabbs.reserve(totalPrims);
    primRefs.reserve(totalPrims);

    for (int i = 0; i < static_cast<int>(numSpheres); ++i) {
        aabbs.push_back(computeAABB(spheres[i]));
        primRefs.push_back({0, i});
    }
    for (int i = 0; i < static_cast<int>(numTriangles); ++i) {
        aabbs.push_back(computeAABB(triangles[i]));
        primRefs.push_back({1, i});
    }

    Log::info("Building BVH over {} spheres + {} triangles", numSpheres, numTriangles);

    const int        n = static_cast<int>(totalPrims);
    std::vector<int> indices(static_cast<std::size_t>(n));
    std::iota(indices.begin(), indices.end(), 0);

    std::vector<Node> tree;
    tree.reserve(static_cast<std::size_t>(2 * n));

    const int treeRoot = buildR(tree, aabbs, indices.data(), indices.data() + n);

    nodes.clear();
    nodes.reserve(tree.size());
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

    // Internal node — meta: x=left, y=right, z=-1, w=skip
    int rightFlat = flatten(node.right, tree, primRefs, nextAfterSubtree);
    int leftFlat = flatten(node.left, tree, primRefs, rightFlat);

    nodes[currentIndex] = {glm::vec4(node.aabb.min, 0.0f), glm::vec4(node.aabb.max, 0.0f), glm::ivec4(leftFlat, rightFlat, -1, nextAfterSubtree)};

    return currentIndex;
}
