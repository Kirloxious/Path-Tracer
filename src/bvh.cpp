#include "bvh.h"
#include <numeric>

AABB computeAABB(const Sphere& s) {
    glm::vec3 rvec(s.radius);
    return { s.position - rvec, s.position + rvec };
}

AABB computeAABB(const Quad& q) {
    AABB aabb{};
    aabb = { q.corner_point, q.corner_point + q.u + q.v};
    aabb.pad(); // in case quad is exactly on bouding box, add some padding
    return aabb;
}

AABB surroundingBox(const AABB& a, const AABB& b) {
    return {
        glm::min(a.min, b.min),
        glm::max(a.max, b.max)
    };
}

float BVH::computeSAHCost(int numLeft, float leftArea, int numRight, float rightArea) {
    // Constant traversal cost = 1.0, intersection cost = 1.0
    return 1.0f + (leftArea * numLeft + rightArea * numRight);
}

// SAH effectivly reduces the number of interesection tests by splitting the aabb into optimal subboxes.
// It does this by finding the best axis to split on using the surface area of the aabbs
void BVH::findBestSplit(const std::vector<AABB>& aabbs, const std::vector<int>& indices, int& splitIndex, std::vector<int>& outSorted, std::vector<AABB>& scratchLeft, std::vector<AABB>& scratchRight)
{
    float bestCost = FLT_MAX;
    int n = (int)indices.size();

    scratchLeft.resize(n);
    scratchRight.resize(n);

    for (int axis = 0; axis < 3; axis++) {
        std::vector<int> sorted = indices;
        std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
            return aabbs[a].center()[axis] < aabbs[b].center()[axis];
        });

        scratchLeft[0] = aabbs[sorted[0]];
        for (int i = 1; i < n; ++i)
            scratchLeft[i] = surroundingBox(scratchLeft[i-1], aabbs[sorted[i]]);

        scratchRight[n-1] = aabbs[sorted[n-1]];
        for (int i = n - 2; i >= 0; --i)
            scratchRight[i] = surroundingBox(scratchRight[i+1], aabbs[sorted[i]]);

        for (int i = 1; i < n; ++i) {
            float cost = computeSAHCost(i,     scratchLeft[i-1].surfaceArea(),
                                        n - i, scratchRight[i].surfaceArea());
            if (cost < bestCost) {
                bestCost   = cost;
                splitIndex = i;
                outSorted  = sorted;
            }
        }
    }
}

int BVH::buildR(std::vector<Node>& tree, const std::vector<AABB>& aabbs, std::vector<int> indices, std::vector<AABB>& scratchLeft, std::vector<AABB>& scratchRight){
    Node node;

    // Compute bounding box over all primitives in this node
    node.aabb = aabbs[indices[0]];
    for (int i = 1; i < (int)indices.size(); ++i)
        node.aabb = surroundingBox(node.aabb, aabbs[indices[i]]);

    // Leaf — single primitive
    if (indices.size() == 1) {
        node.primitiveIndex = indices[0];
        tree.push_back(node);
        return (int)tree.size() - 1;
    }

    // Interior — find best SAH split and recurse
    int              splitIndex;
    std::vector<int> sorted;
    findBestSplit(aabbs, indices, splitIndex, sorted, scratchLeft, scratchRight);

    std::vector<int> leftIndices (sorted.begin(), sorted.begin() + splitIndex);
    std::vector<int> rightIndices(sorted.begin() + splitIndex, sorted.end());

    node.left  = buildR(tree, aabbs, std::move(leftIndices),  scratchLeft, scratchRight);
    node.right = buildR(tree, aabbs, std::move(rightIndices), scratchLeft, scratchRight);

    tree.push_back(node);
    return (int)tree.size() - 1;
}

void BVH::build(const std::vector<Sphere>& spheres){
    std::vector<AABB> aabbs;
    aabbs.reserve(spheres.size());
    for (const auto& sphere : spheres) {
        aabbs.push_back(computeAABB(sphere));
    }
    std::cout << "Number of spheres: " << spheres.size() << std::endl;

    
    std::vector<int> indices(spheres.size());
    std::iota(indices.begin(), indices.end(), 0); // [0, 1, 2, ..., N]
    
    int n = (int)indices.size();
    std::vector<Node> tree;
    tree.reserve(2 * n);

    std::vector<AABB> scratchLeft(n), scratchRight(n);
    int treeRoot = buildR(tree, aabbs, std::move(indices), scratchLeft, scratchRight);

    nodes.clear();
    nodes.reserve(tree.size());
    root = flatten(treeRoot, tree, -1);
    
} 

int BVH::flatten(int nodeIndex, const std::vector<Node>& tree, int nextAfterSubtree) {
    if (nodeIndex < 0) return nextAfterSubtree;

    const Node& node = tree[nodeIndex];
    int currentIndex = static_cast<int>(nodes.size());
    nodes.emplace_back();

    
    // Leaf node
    if (node.isLeaf()) {
        nodes[currentIndex] = {
            glm::vec4(node.aabb.min, 0.0f),
            glm::vec4(node.aabb.max, 0.0f),
            glm::ivec4(-1, -1, node.primitiveIndex, nextAfterSubtree)
        };
        return currentIndex;
    }


    // Internal node
    int rightFlat = flatten(node.right, tree, nextAfterSubtree);
    int leftFlat  = flatten(node.left,  tree, rightFlat);

    nodes[currentIndex] = {
        glm::vec4(node.aabb.min, 0.0f),
        glm::vec4(node.aabb.max, 0.0f),
        glm::ivec4(leftFlat, rightFlat, -1, nextAfterSubtree)
    };

    return currentIndex;
}
