#pragma once

#include <atomic>
#include <span>
#include <vector>
#include <glm/glm.hpp>

#include "scene/primitive.h"

struct AABB
{
    glm::vec3 min{};
    glm::vec3 max{};

    [[nodiscard]] glm::vec3 center() const { return (min + max) * 0.5f; }

    [[nodiscard]] float surfaceArea() const {
        glm::vec3 d = max - min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    /// Pads each axis independently, not by surface area: axis-aligned walls would otherwise keep a
    /// zero-thickness slab and fail the slab test's `tNear < tFar`.
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

/// Not padded.
[[nodiscard]] AABB surroundingBox(const AABB& a, const AABB& b);

/// Already pad()-ed.
[[nodiscard]] AABB computeAABB(const Triangle& t, std::span<const Vertex> vertices);

/// Batching a few triangles per leaf amortises the node fetch and slab test one-triangle leaves pay each.
inline constexpr int MAX_LEAF_TRIANGLES = 4;

/// Two vec4s with int bits in the unused `.w` lanes (two nodes per cache line). The left child is
/// implicit (next slot). Interior: min.w = right child, max.w = 0. Leaf: min.w = first ref, max.w = count > 0.
struct alignas(16) BVHNodeFlat
{
    glm::vec4 aabbMin;
    glm::vec4 aabbMax;
};
static_assert(sizeof(BVHNodeFlat) == 32, "BVH node must stay 32 bytes to keep two per cache line");

class BVH
{
public:
    std::vector<BVHNodeFlat> nodes;

    /// Leaf -> triangle indirection, so leaves can batch triangles without reordering `World::triangles`,
    /// whose emissive-first order the light groups depend on.
    std::vector<int> triRefs;

    /// -1 if never built; otherwise 0, since flatten() emits the root first.
    int root = -1;

    /// Deepest root-to-leaf path; the GPU traversal stack must be at least this deep.
    int maxDepth = 0;

    /// Must run after World::sortEmissiveFirst(), since `triRefs` stores triangle indices. @p triangles must not be empty.
    void build(std::span<const Triangle> triangles, std::span<const Vertex> vertices);

private:
    struct Node
    {
        AABB aabb{};
        int  left = -1;
        int  right = -1;
        int  firstRef = -1;
        int  refCount = 0;
        int  subtreeSize = 1; ///< Includes this node; used to place sibling subtrees.

        [[nodiscard]] bool isLeaf() const { return refCount > 0; }
    };

    static constexpr int NUM_BINS = 16;

    struct Bin
    {
        AABB aabb{};
        int  count = 0;
    };

    /// Partitions @p range in place. @p nextSlot is atomic because large subtrees build concurrently.
    [[nodiscard]] static int buildR(std::vector<Node>& tree, std::atomic<int>& nextSlot, std::span<const AABB> aabbs, const int* refsBase,
                                    std::span<const glm::vec3> centroids, std::span<int> range);

    /// Emits [self, left, right] depth-first, which makes the left child implicitly `self + 1`.
    [[nodiscard]] int flatten(int nodeIndex, const std::vector<Node>& tree, int depth);
};
