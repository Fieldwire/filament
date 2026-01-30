/*
 * TinyBVHPicking.h - Wrapper to use tinybvh library for accelerated ray-triangle picking
 */

#ifndef GLTFIO_TINYBVH_PICKING_H
#define GLTFIO_TINYBVH_PICKING_H

#include <gltfio/Picking.h>
#include <utils/Entity.h>
#include <math/vec3.h>

namespace filament::gltfio {

/**
 * TinyBVH-accelerated picking registry.
 * Drop-in replacement for the custom BVH in PickingRegistry.
 * Uses custom intersection callbacks to properly handle hidden triangles.
 */
class TinyBVHPickingRegistry {
public:
    struct Hit {
        utils::Entity entity;
        int triangle;     // Triangle index within the mesh
        float distance;   // Ray parameter t
        filament::math::float3 bary; // Barycentric coordinates
    };

    TinyBVHPickingRegistry() = default;
    ~TinyBVHPickingRegistry();

    /**
     * Register a mesh for picking. Builds a tinybvh BVH structure.
     * @param e Entity handle
     * @param mesh MeshData containing positions and indices
     */
    void registerMesh(utils::Entity e, MeshData&& mesh);

    /**
     * Update the world transform for an entity.
     */
    void updateTransform(utils::Entity e, const filament::math::mat4f& world);

    /**
     * Perform ray picking against all registered meshes.
     * @param rayOrigin Ray origin in world space
     * @param rayDir Ray direction in world space (normalized)
     * @return Hit information (entity.isNull() if no hit)
     */
    Hit pick(const filament::math::float3& rayOrigin,
             const filament::math::float3& rayDir) const;

    /**
     * Perform ray picking, skipping triangles whose index positions fall in [startIdx, endIdx].
     * Uses custom intersection callbacks to properly handle hidden triangles that are
     * closer to the camera than visible ones.
     *
     * @param rayOrigin Ray origin in world space
     * @param rayDir Ray direction in world space (normalized)
     * @param startIdx Start of hidden index range (inclusive)
     * @param endIdx End of hidden index range (inclusive)
     * @return Hit information (entity.isNull() if no hit)
     */
    Hit pickSkippingIndexRange(const filament::math::float3& rayOrigin,
                               const filament::math::float3& rayDir,
                               uint32_t startIdx, uint32_t endIdx) const;

    /**
     * Get mesh data for an entity (read-only).
     */
    const MeshData* getMesh(utils::Entity e) const;

private:
    struct EntityMesh;
    std::unordered_map<utils::Entity, std::unique_ptr<EntityMesh>> mMeshes;
    std::unordered_map<utils::Entity, filament::math::mat4f> mWorldTransforms;
};

} // namespace filament::gltfio

#endif // GLTFIO_TINYBVH_PICKING_H

