#pragma once

/**
 * @file world.h
 * @brief Geometry container and scene-building API: vertices, triangles, materials, lights, BVH.
 */

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "scene/bvh.h"
#include "scene/material.h"
#include "scene/obj_loader.h"

/**
 * @brief Owns all scene geometry and the acceleration structure built over it.
 *
 * Scene factories call the `add*` builders in any order, then World::sortEmissiveFirst()
 * followed by World::create(). Every member vector is uploaded verbatim to an SSBO by
 * Renderer::loadScene(), so element layouts are part of the GPU contract.
 */
class World
{
public:
    /**
     * @brief One area light: a run of consecutive emissive triangles sharing a material.
     *
     * `(begin, count, total_area)` — one entry per source emissive primitive. Stored as a
     * 16-byte struct so the std430 upload matches the GLSL declaration exactly.
     */
    struct alignas(16) LightGroup
    {
        int32_t begin = 0;         ///< Index of the group's first triangle in `triangles`.
        int32_t count = 0;         ///< Number of triangles in the group.
        float   total_area = 0.0f; ///< Summed area, used to convert the area pdf.
        float   _pad = 0.0f;
    };

    std::vector<Vertex>     vertices;
    std::vector<Triangle>   triangles;
    std::vector<Material>   materials;
    std::vector<LightGroup> lightGroups;
    BVH                     bvh;
    /// Index of the last emissive triangle after sortEmissiveFirst(), or -1 when the scene has
    /// no emitters. The shader treats `[0, emissiveLastIndex]` as the NEE candidate range.
    int emissiveLastIndex = -1;

    /**
     * @brief Appends a material.
     * @param mat Material to store; moved in.
     * @return Its index, for use as a `material_index` on subsequent geometry.
     */
    uint32_t addMaterial(Material mat);

    /**
     * @brief Appends a single vertex.
     * @param position       World-space position.
     * @param normal         Surface normal; expected normalized.
     * @param material_index Index into `materials`. Must match every triangle sharing this
     *                       vertex — the raster pass reads it as a flat varying.
     * @return The new vertex's index, for use with makeTriangle().
     */
    uint32_t addVertex(glm::vec3 position, glm::vec3 normal, uint32_t material_index);

    /**
     * @brief Tessellates a UV sphere into triangles and appends them.
     *
     * There is no analytic sphere primitive on the GPU — spheres become triangles here.
     * Per-vertex normals are the exact analytic ones, so shading stays smooth; pole rows emit
     * one triangle per longitude (the other would be degenerate). Triangles are wound CCW
     * relative to the outward normal, which the light-pdf and ReSTIR target-pdf code relies on.
     *
     * The default density (16 longitude x 8 latitude = 224 triangles) trades silhouette
     * quality against BVH cost. Tiny or barely visible spheres should pass a lower density —
     * every triangle ends up in the scene BVH.
     *
     * @param center         Sphere centre in world space.
     * @param radius         Sphere radius.
     * @param material_index Index into `materials`, applied to every generated vertex.
     * @param latSegs        Latitude segments; clamped to a minimum of 2.
     * @param lonSegs        Longitude segments; clamped to a minimum of 3.
     */
    void addSphere(glm::vec3 center, float radius, uint32_t material_index, int latSegs = 8, int lonSegs = 16);

    /**
     * @brief addSphere() overload that registers @p mat first.
     * @param center  Sphere centre in world space.
     * @param radius  Sphere radius.
     * @param mat     Material to append via addMaterial() and apply to the sphere.
     * @param latSegs Latitude segments; clamped to a minimum of 2.
     * @param lonSegs Longitude segments; clamped to a minimum of 3.
     */
    void addSphere(glm::vec3 center, float radius, Material mat, int latSegs = 8, int lonSegs = 16);

    /**
     * @brief Appends one triangle with a flat geometric normal.
     * @param v0             First corner.
     * @param v1             Second corner.
     * @param v2             Third corner. Wind CCW about the intended outward normal.
     * @param material_index Index into `materials`.
     */
    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, uint32_t material_index);

    /**
     * @brief addTriangle() overload that registers @p mat first.
     * @param v0  First corner.
     * @param v1  Second corner.
     * @param v2  Third corner.
     * @param mat Material to append via addMaterial() and apply.
     */
    void addTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Material mat);

    /**
     * @brief Appends a parallelogram as two triangles.
     *
     * Both triangles are wound CCW around `cross(u, v)`, so swapping @p u and @p v flips which
     * way the quad faces — which matters for emissive quads and for the mirror floor.
     *
     * @param corner         Origin corner of the parallelogram.
     * @param u              First edge vector from @p corner.
     * @param v              Second edge vector from @p corner.
     * @param material_index Index into `materials`.
     */
    void addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, uint32_t material_index);

    /**
     * @brief addTriQuad() overload that registers @p mat first.
     * @param corner Origin corner of the parallelogram.
     * @param u      First edge vector from @p corner.
     * @param v      Second edge vector from @p corner.
     * @param mat    Material to append via addMaterial() and apply.
     */
    void addTriQuad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, Material mat);

    /**
     * @brief Appends a loaded OBJ mesh, rebasing its indices onto `vertices`.
     * @param mesh Mesh from loadOBJ(). An empty mesh (a failed load) contributes nothing.
     */
    void addMesh(const OBJMesh& mesh);

    /**
     * @brief Finalizes the world: validates, coalesces light groups, builds the BVH.
     *
     * Must run *after* sortEmissiveFirst() so both the light groups and the BVH see the
     * post-sort triangle order. If validate() rejects the world, the light groups and BVH are
     * skipped rather than built on unusable data.
     */
    void create();

    /**
     * @brief Stable-partitions emissive triangles to the front of `triangles`.
     *
     * Records the last emissive index in `emissiveLastIndex`. The shader's NEE light selection
     * assumes a contiguous emissive prefix, so this must run before create().
     */
    void sortEmissiveFirst();

    /**
     * @brief Checks that the world is usable and reports any problems through Log::.
     *
     * Index-range and emissive-sort problems are reported but do not fail the check — they
     * degrade the image rather than crashing the builder.
     *
     * @return false when the world is unusable (no geometry or no materials) — callers must
     *         not proceed to buildLightGroups() or bvh.build() in that case.
     */
    [[nodiscard]] bool validate() const;

private:
    /**
     * @brief Coalesces emissive triangles into LightGroups and bakes their sampling CDF.
     *
     * Groups consecutive emissive triangles that share a material into a single light, then
     * writes an area-weighted cumulative CDF into each emissive triangle's `cdf_in_group`.
     * NEE samples a group uniformly, then binary-searches the CDF to pick a triangle
     * proportional to area — making a tessellated sphere or quad behave like one uniform area
     * light regardless of how its triangles are sized (poles vs. equator on a sphere).
     *
     * Relies on every emissive source being added with a single material — true for
     * addSphere() and addTriQuad(). A scene wanting two distinct lights with the same
     * appearance should duplicate the material.
     */
    void buildLightGroups();
};
