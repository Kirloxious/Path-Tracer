#pragma once

#include <vector>
#include <iostream>
#include <algorithm>
#include <glm/glm.hpp>

#include "primitive.h"

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    
    glm::vec3 center() const {
        return (min + max) * 0.5f;
    }
    
    AABB expand(float delta) const {
        float padding = delta/2;
        return { glm::vec3(min.x - padding, min.y - padding, min.z - padding), 
            glm::vec3(max.x + padding, max.y + padding, max.z + padding) };
        }
        
    float surfaceArea() const {
            glm::vec3 d = max - min;
            return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
        }
        
    };
    
void padAABB(AABB& aabb) {
    float delta = 0.001f;
    if(aabb.surfaceArea() < delta){
        aabb = aabb.expand(delta);
    }
}

AABB computeAABB(const Sphere& s) {
    glm::vec3 rvec(s.radius);
    return { s.position - rvec, s.position + rvec };
}

AABB computeAABB(const Quad& q) {
    AABB aabb{};
    aabb = { q.corner_point, q.corner_point + q.u + q.v};
    padAABB(aabb); // in case quad is exactly on bouding box, add some padding
    return aabb;
}

AABB surroundingBox(const AABB& a, const AABB& b) {
    return {
        glm::min(a.min, b.min),
        glm::max(a.max, b.max)
    };
}


struct BVHNode {
    AABB aabb;
    int left;
    int right;
    int sphereIndex = -1;
};

struct alignas(16) BVHNodeFlat {
    glm::vec4 aabbMin;   // .xyz = min
    glm::vec4 aabbMax;   // .xyz = max
    glm::ivec4 meta;     // .x = left, .y = right, .z = sphereIndex, .w = unused
};

AABB computeSceneAABB(const std::vector<Sphere>& spheres) {
    glm::vec3 minPoint(std::numeric_limits<float>::max());
    glm::vec3 maxPoint(std::numeric_limits<float>::lowest());

    for (const Sphere& sphere : spheres) {
        glm::vec3 center = sphere.position;

        minPoint = glm::min(minPoint, center);
        maxPoint = glm::max(maxPoint, center);
    }

    AABB bounds;
    bounds.min = minPoint;
    bounds.max = maxPoint;
    return bounds;
}

int findMaxVarianceAxis(const std::vector<int> &sphereIndices, const std::vector<AABB> &aabbs){
    float mean[3] = {0}, var[3] = {0};
    for (int idx : sphereIndices) {
        glm::vec3 c = aabbs[idx].center();
        mean[0] += c.x;
        mean[1] += c.y;
        mean[2] += c.z;
    }
    int n = sphereIndices.size();
    mean[0] /= n; mean[1] /= n; mean[2] /= n;

    for (int idx : sphereIndices) {
        glm::vec3 c = aabbs[idx].center();
        var[0] += (c.x - mean[0]) * (c.x - mean[0]);
        var[1] += (c.y - mean[1]) * (c.y - mean[1]);
        var[2] += (c.z - mean[2]) * (c.z - mean[2]);
    }

    int axis = 0;
    if (var[1] > var[0]) axis = 1;
    if (var[2] > var[axis]) axis = 2;

    return axis;
}

// Split along the longest axis
int longestAxis(const AABB &box){
    glm::vec3 extent = box.max - box.min;
    int axis;
    if (extent.x > extent.y && extent.x > extent.z)
        axis = 0;
    else if (extent.y > extent.z)
        axis = 1;
    else
        axis = 2;

    return axis;
}


float computeSAHCost(int numLeft, float leftArea, int numRight, float rightArea) {
    // Constant traversal cost = 1.0, intersection cost = 1.0
    return 1.0f + (leftArea * numLeft + rightArea * numRight);
}

// SAH effectivly reduces the number of interesection tests by splitting the aabb into optimal subboxes.
// It does this by finding the best axis to split on using the surface area of the aabbs
int findBestSAHSplit(const std::vector<AABB>& aabbs, const std::vector<int>& indices, int& splitAxis, int& splitIndex) {
    float bestCost = FLT_MAX;

    // Iterate over all axis
    for (int axis = 0; axis < 3; axis++) {
        // Sort by axis 
        std::vector<int> sorted = indices;
        std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
            return aabbs[a].center()[axis] < aabbs[b].center()[axis];
        });

        // Compute left and right boxes on current axis
        std::vector<AABB> leftBoxes(sorted.size()), rightBoxes(sorted.size());
        AABB leftBox = aabbs[sorted[0]];
        for (int i = 1; i < (int)sorted.size(); ++i)
            leftBoxes[i] = leftBox = surroundingBox(leftBox, aabbs[sorted[i]]);

        AABB rightBox = aabbs[sorted.back()];
        for (int i = (int)sorted.size() - 2; i >= 0; --i)
            rightBoxes[i] = rightBox = surroundingBox(rightBox, aabbs[sorted[i]]);

        // Compute the SAH cost which uses the surface area of the left and right boxes
        for (int i = 1; i < (int)sorted.size(); ++i) {
            float leftArea = leftBoxes[i].surfaceArea();
            float rightArea = rightBoxes[i].surfaceArea();

            float cost = computeSAHCost(i, leftArea, sorted.size() - i, rightArea);
            if (cost < bestCost) {
                bestCost = cost;
                splitAxis = axis;
                splitIndex = i;
            }
        }
    }

    return splitAxis;
}


int buildBVH(std::vector<BVHNode>& bvh, const std::vector<Sphere>& spheres, const std::vector<AABB>& aabbs, std::vector<int> sphereIndices) {
    BVHNode node;

    // Compute bounding box for all spheres in this node
    AABB box = aabbs[sphereIndices[0]];
    for (size_t i = 1; i < sphereIndices.size(); i++) {
        box = surroundingBox(box, aabbs[sphereIndices[i]]);
    }

    node.aabb = box;

    if (sphereIndices.size() == 1) {
        node.sphereIndex = sphereIndices[0];
        node.left = node.right = -1;
        bvh.push_back(node);
        return bvh.size() - 1;
    }

    int axis, splitIndex;
    findBestSAHSplit(aabbs, sphereIndices, axis, splitIndex);

    std::sort(sphereIndices.begin(), sphereIndices.end(), [&](int a, int b) {
        return aabbs[a].center()[axis] < aabbs[b].center()[axis];
    });

    std::vector<int> leftIndices(sphereIndices.begin(), sphereIndices.begin() + splitIndex);
    std::vector<int> rightIndices(sphereIndices.begin() + splitIndex, sphereIndices.end());

    int leftChild = buildBVH(bvh, spheres, aabbs, leftIndices);
    int rightChild = buildBVH(bvh, spheres, aabbs, rightIndices);

    node.left = leftChild;
    node.right = rightChild;
    bvh.push_back(node);
    return bvh.size() - 1;
}
struct MortonPrimitive{
    uint32_t code;
    int index;
};

uint32_t expandBits(uint32_t v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

uint32_t morton3D(float x, float y, float z) {
    x = glm::clamp(x * 1024.0f, 0.0f, 1023.0f);
    y = glm::clamp(y * 1024.0f, 0.0f, 1023.0f);
    z = glm::clamp(z * 1024.0f, 0.0f, 1023.0f);

    return (expandBits((uint32_t)x) << 2) |
            (expandBits((uint32_t)y) << 1) |
            expandBits((uint32_t)z);
}


int findSplit(const std::vector<MortonPrimitive>& mortonPrims, int first, int last) {
    uint32_t firstCode = mortonPrims[first].code;
    uint32_t lastCode  = mortonPrims[last - 1].code; 

    if (firstCode == lastCode) {
        return (first + last) >> 1;
    }

    int commonPrefix = __builtin_clz(firstCode ^ lastCode);

    int split = first;
    int step = last - first;

    while (step > 1) {
        int mid = split + (step >> 1);
        uint32_t midCode = mortonPrims[mid].code;
        int midPrefix = __builtin_clz(firstCode ^ midCode);

        if (midPrefix > commonPrefix)
            split = mid;

        step >>= 1;
    }

    return split;
}


int buildLBVH(std::vector<BVHNode>& nodes, const std::vector<AABB>& aabbs, const std::vector<MortonPrimitive>& mortonPrims, int start, int end) {
    if (end - start == 1) {
        // Leaf node
        BVHNode leaf;
        leaf.sphereIndex = mortonPrims[start].index;
        leaf.left = leaf.right = -1;
        leaf.aabb = aabbs[leaf.sphereIndex];
        nodes.push_back(leaf);
        return nodes.size() - 1;
    }

    int split = findSplit(mortonPrims, start, end); // end is exclusive

    int left  = buildLBVH(nodes, aabbs, mortonPrims, start, split + 1);
    int right = buildLBVH(nodes, aabbs, mortonPrims, split + 1, end);

    BVHNode node;
    node.left = left;
    node.right = right;
    node.sphereIndex = -1;
    node.aabb = surroundingBox(nodes[left].aabb, nodes[right].aabb);
    nodes.push_back(node);
    return nodes.size() - 1;
}

int flattenBVH(int nodeIndex, const std::vector<BVHNode>& nodes, std::vector<BVHNodeFlat>& flatNodes, int nextAfterSubtree) {
    if (nodeIndex < 0) return nextAfterSubtree;

    const BVHNode& node = nodes[nodeIndex];
    int currentIndex = static_cast<int>(flatNodes.size());
    flatNodes.emplace_back();

    
    // Leaf node
    if (node.sphereIndex != -1) {
        BVHNodeFlat& flat = flatNodes[currentIndex];
        flat.aabbMin = glm::vec4(node.aabb.min, 0.0f);
        flat.aabbMax = glm::vec4(node.aabb.max, 0.0f);
        flat.meta = glm::ivec4(-1, -1, node.sphereIndex, nextAfterSubtree);
        return currentIndex;
    }

    // Internal node
    int rightFlatIndex = flattenBVH(node.right, nodes, flatNodes, nextAfterSubtree);
    int leftFlatIndex = flattenBVH(node.left, nodes, flatNodes, rightFlatIndex);

    BVHNodeFlat& flat = flatNodes[currentIndex];
    flat.aabbMin = glm::vec4(node.aabb.min, 0.0f);
    flat.aabbMax = glm::vec4(node.aabb.max, 0.0f);
    flat.meta = glm::ivec4(leftFlatIndex, rightFlatIndex, -1, nextAfterSubtree);

    return currentIndex;
}
