/*
 * TinyBVHPicking.h - Wrapper to use tinybvh library for accelerated ray-triangle picking
 */

#ifndef GLTFIO_TINYBVH_PICKING_REGISTRY_H
#define GLTFIO_TINYBVH_PICKING_REGISTRY_H

#include "Picking.h"
#include <utils/Entity.h>
#include <math/vec3.h>
#include <math/mat4.h>
#include <unordered_map>
#include <memory>

namespace filament::gltfio {

/**
 * TinyBVH-accelerated picking registry.
 * Drop-in replacement for the custom BVH in PickingRegistry.
 * Uses custom intersection callbacks to properly handle hidden triangles.
 */
class UTILS_PUBLIC TinyBVHPickingRegistry {
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

    Hit *pick(
        View *view, const int2 &position, FilamentAsset *asset
    );

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

    Hit *pickSkippingIndexRange(
        View *view, const int2 &position, FilamentAsset *asset,
        uint32_t startIdx, uint32_t endIdx
    );

    /**
     * Get mesh data for an entity (read-only).
     */
    const MeshData* getMesh(utils::Entity e) const;

    /**
     * Get the number of registered meshes.
     * Useful for debugging - check this is > 0 before picking.
     */
    size_t getMeshCount() const { return mMeshes.size(); }

private:
    // Forward declaration; defined in TinyBVHPicking.cpp
    struct EntityMesh;
    // Custom deleter so unique_ptr doesn't require a complete type here
    struct EntityMeshDeleter {
        void operator()(EntityMesh* p) const;
    };

    // Use utils::Entity hasher; rely on default std::equal_to for KeyEqual
    std::unordered_map<utils::Entity,
                       std::unique_ptr<EntityMesh, EntityMeshDeleter>,
                       utils::Entity::Hasher> mMeshes;

    std::unordered_map<utils::Entity,
                       filament::math::mat4f,
                       utils::Entity::Hasher> mWorldTransforms;
};

} // namespace filament::gltfio

#endif // GLTFIO_TINYBVH_PICKING_REGISTRY_H
