#pragma once

/**
 * @file bvh.h
 * @brief Binned-SAH bounding volume hierarchy, flattened for stackless GPU traversal.
 */

#include <atomic>
#include <span>
#include <vector>
#include <glm/glm.hpp>

#include "scene/primitive.h"

/// @brief Axis-aligned bounding box.
struct AABB
{
    glm::vec3 min{};
    glm::vec3 max{};

    /// @return The midpoint of the box.
    [[nodiscard]] glm::vec3 center() const { return (min + max) * 0.5f; }

    /// @return Total surface area of the box — the SAH cost metric.
    [[nodiscard]] float surfaceArea() const {
        glm::vec3 d = max - min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    /**
     * @brief Expands any axis thinner than a small epsilon, in place.
     *
     * Pads each axis *independently* rather than by surface area. This is critical for
     * axis-aligned geometry such as Cornell-box walls: a surface-area-based pad would leave
     * a zero-thickness slab on one axis and break the slab test's `tNear < tFar` comparison.
     *
     * @return `*this`, so the call can be chained onto a freshly computed box.
     */
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

/**
 * @brief Computes the box enclosing both inputs.
 * @param a First box.
 * @param b Second box.
 * @return The union box. Not padded.
 */
[[nodiscard]] AABB surroundingBox(const AABB& a, const AABB& b);

/**
 * @brief Computes a triangle's padded bounding box from the indexed vertex pool.
 * @param t        Triangle whose `indices` are looked up in @p vertices.
 * @param vertices Vertex pool; every index in @p t must be in range.
 * @return The triangle's AABB, already pad()-ed.
 */
[[nodiscard]] AABB computeAABB(const Triangle& t, const std::vector<Vertex>& vertices);

/**
 * @brief One flattened BVH node as uploaded to the BVHBuffer SSBO (binding 3).
 *
 * 48 bytes, `alignas(16)`. `meta.w` is the skip pointer that makes traversal stackless: on a
 * missed box the shader jumps straight to that index instead of maintaining a stack.
 */
struct alignas(16) BVHNodeFlat
{
    glm::vec4  aabbMin; ///< .xyz = box min, .w unused.
    glm::vec4  aabbMax; ///< .xyz = box max, .w unused.
    glm::ivec4 meta;    ///< interior: .x = left, .y = right, .z = -1,         .w = skip
                        ///< leaf:     .x = -1,   .y = 0,     .z = triangleIdx, .w = skip
};

/**
 * @brief Binned-SAH BVH over triangle AABBs, flattened into a GPU-traversable array.
 *
 * Built once per scene by World::create(). The construction recursion parallelizes its two
 * subtree calls above a primitive-count threshold, so only the top few levels spawn threads.
 */
class BVH
{
public:
    /// Depth-first-ordered flattened nodes, uploaded verbatim to binding 3.
    std::vector<BVHNodeFlat> nodes;
    /// Index of the entry node in `nodes`, or -1 if the tree was never built.
    int root = -1;

    /**
     * @brief Builds the hierarchy and flattens it into `nodes`.
     *
     * Splits with a 16-bin surface-area heuristic over triangle centroids. Ranges of four or
     * fewer primitives take a median split on the longest axis instead, and a median split is
     * also the fallback when all centroids coincide or the best binned partition puts every
     * primitive on one side.
     *
     * @param triangles Triangles to index, in their final order — this must run *after*
     *                  World::sortEmissiveFirst(), since leaves store triangle indices.
     *                  Must not be empty (debug builds assert; release builds are UB).
     * @param vertices  Vertex pool the triangle AABBs are derived from.
     */
    void build(const std::vector<Triangle>& triangles, const std::vector<Vertex>& vertices);

private:
    /// Intermediate pointer-style node used during construction, before flattening.
    struct Node
    {
        AABB aabb{};
        int  left = -1;
        int  right = -1;
        int  primitiveIndex = -1; ///< >= 0 on a leaf; -1 on an interior node.
        int  subtreeSize = 1;     ///< Node count including this one — used to derive skip pointers.

        /// @return true when this node stores a primitive rather than two children.
        [[nodiscard]] bool isLeaf() const { return primitiveIndex != -1; }
    };

    /// Number of SAH bins evaluated per axis.
    static constexpr int NUM_BINS = 16;

    /// One SAH bin: the union box of the triangles whose centroid fell into it, and how many did.
    struct Bin
    {
        AABB aabb{};
        int  count = 0;
    };

    /**
     * @brief Recursively builds one subtree and writes its nodes into @p tree.
     *
     * @p range is the slice of the shared index array this subtree owns; buildR partitions it
     * in place and hands each half to a recursive call.
     *
     * @param tree      Preallocated node storage, written through @p nextSlot.
     * @param nextSlot  Shared bump allocator into @p tree; atomic because large subtrees are
     *                  built concurrently.
     * @param aabbs     Per-triangle bounding boxes, indexed by the values in @p range.
     * @param centroids Per-triangle centroids, indexed the same way.
     * @param range     Triangle indices owned by this subtree; must not be empty. Reordered
     *                  in place.
     * @return Index in @p tree of the node created for this subtree.
     */
    [[nodiscard]] static int buildR(std::vector<Node>& tree, std::atomic<int>& nextSlot, const std::vector<AABB>& aabbs,
                                    const std::vector<glm::vec3>& centroids, std::span<int> range);

    /**
     * @brief Depth-first flattens a built subtree into `nodes`, filling in skip pointers.
     *
     * @param nodeIndex        Node in @p tree to emit.
     * @param tree             The built intermediate hierarchy.
     * @param nextAfterSubtree Flat index to jump to when this subtree's box is missed — the
     *                         skip pointer stored in `meta.w`.
     * @return The flat index this node was written to.
     */
    [[nodiscard]] int flatten(int nodeIndex, const std::vector<Node>& tree, int nextAfterSubtree);
};
