/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GLTFIO_PICKING_REGISTRY_H
#define GLTFIO_PICKING_REGISTRY_H

#include <math/vec3.h>
#include <utils/Entity.h>

#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

// Forward declaration of cgltf types
struct cgltf_mesh;

// Forward declaration of tinybvh BVH
namespace tinybvh {
    class BVH;
    struct bvhvec4;
}

// Forward declaration of Filament types
namespace filament {
    class View;
    class Camera;
    class TransformManager;
}

namespace filament::gltfio {

/**
 * MeshData stores the geometry data and BVH for fast ray-triangle intersection.
 */
struct MeshData {
    std::vector<math::float3> positions;        // Vertex positions in local space
    std::vector<uint32_t> indices;              // Triangle indices (3 per triangle)
    std::unique_ptr<tinybvh::BVH> bvh;          // BVH for accelerated ray tracing
    std::vector<tinybvh::bvhvec4> bvhVertices;  // Per-vertex data should be persisted, because tinybvh stores a pointer to this

    // For AssetLoaderExtended:
    // When vertices are duplicated during tangent generation, the index buffer is remapped.
    // We store these expanded indices so triangle filtering can work correctly.
    // The vertex buffer itself is reused from the renderable - we don't need to store positions.
    std::vector<uint32_t> expandedIndices;       // Remapped indices for expanded vertex buffer

    MeshData() = default;
    MeshData(MeshData&&) noexcept = default;
    MeshData& operator=(MeshData&&) noexcept = default;

    // Disable copy to avoid expensive copies
    MeshData(const MeshData&) = delete;
    MeshData& operator=(const MeshData&) = delete;
};

/**
 * Hit result from ray-triangle intersection test.
 */
struct PickingHit {
    utils::Entity entity;           // Entity that was hit
    int triangleIndex = -1;         // Index of the hit triangle (-1 if no hit)
    float distance = 0.0f;          // Distance along the ray to the hit point
};

/**
 * PickingRegistry manages mesh data for entities and builds BVH acceleration structures.
 *
 * This class stores geometry data and BVH for each entity, enabling fast spatial queries.
 */
class PickingRegistry {
public:
    PickingRegistry();
    ~PickingRegistry();

    // Disable copy operations
    PickingRegistry(const PickingRegistry&) = delete;
    PickingRegistry& operator=(const PickingRegistry&) = delete;

    // Enable move operations (implemented in .cpp where MeshData is complete)
    PickingRegistry(PickingRegistry&&) noexcept;
    PickingRegistry& operator=(PickingRegistry&&) noexcept;

    // Defer mesh registration for picking until ResourceLoader has loaded cgltf buffers
    // At this point, buffer->data is NULL, so we can't read vertex positions yet
    // Store the entity-mesh pair to be processed later in ResourceLoader::loadResources()
    void enqueueMesh(utils::Entity entity, const cgltf_mesh* mesh) {
        pendingRegistrations.emplace_back(entity, mesh);
    }

    /**
     * Register all queued meshes into the registry, then clear the queue.
     * Callbacks allow callers to provide mesh data without exposing MeshCache to this header.
     *
     * @param gltfMeshes       base pointer for mesh index arithmetic
     * @param meshCacheSize    number of entries in the mesh cache
     * @param getPrimsSize     (meshIndex) -> number of primitives for that mesh
     * @param getExpandedIndices (meshIndex, primIndex) -> reference to expandedIndices vector for that primitive
     * @return true if all meshes registered successfully; false if any failed (sets mBvhBuildFailed on asset)
     */
    bool commitRegistrations(
        const cgltf_mesh* gltfMeshes,
        size_t meshCacheSize,
        const std::function<size_t(size_t)>& getPrimsSize,
        const std::function<std::vector<uint32_t>&(size_t, size_t)>& getExpandedIndices
    );

    /**
     * Register a mesh from a GLTF mesh structure.
     * Extracts geometry data from the GLTF mesh and builds the BVH.
     *
     * @param entity The entity to associate with this mesh
     * @param mesh The GLTF mesh to extract geometry from
     * @return true if registration was successful, false otherwise
     */
    bool registerMesh(utils::Entity entity, const cgltf_mesh* mesh);

    /**
     * Register expanded indices for an entity (used by AssetLoaderExtended).
     * When vertices are duplicated during tangent generation, the renderable's index buffer
     * is remapped to reference the expanded vertices. This method stores those remapped indices
     * so triangle filtering can work correctly with the expanded vertex layout.
     *
     * @param entity The entity to update
     * @param expandedIndices Remapped indices that match the renderable's vertex buffer
     * @param indexCount Number of indices
     * @return true if successful, false if entity not found
     */
    bool registerExpandedIndices(utils::Entity entity,
                                  const uint32_t* expandedIndices, size_t indexCount);

    /**
     * Perform ray-triangle intersection test against a specific entity's mesh.
     * Computes ray from screen coordinates and optionally skips triangles in specified ranges.
     *
     * @param view The View to use for screen-to-ray conversion
     * @param tcm TransformManager for entity transforms
     * @param entity The entity to test against
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param sortedTriangleSkipRanges Flat array of triangle index ranges to skip during intersection.
     *                         Format: [start0, end0, start1, end1, ...] where each pair defines
     *                         an inclusive range of triangle indices.
     *                         Example: [0, 5, 10, 15] skips triangles 0-5 and 10-15 (inclusive).
     *                         Must be sorted by start index. Can be nullptr for no skipping.
     * @param skipRangeCount Number of range PAIRS in sortedSkipRanges (NOT total array length).
     *                       Example: skipRanges=[0,5,10,15] has skipRangeCount=2
     * @return Hit information with triangleIndex=-1 if no hit
     *
     * Example usage:
     *   // Skip triangle 5 and triangles 10-20
     *   uint32_t sortedTriangleSkipRanges[] = {5, 5, 10, 20};
     *   auto hit = registry.pick(view, tcm, entity, x, y, skipRanges, 2);
     */
    PickingHit pick(View& view,
                    TransformManager& tcm,
                    utils::Entity entity,
                    int screenX,
                    int screenY,
                    const uint32_t* sortedTriangleSkipRanges = nullptr,
                    size_t skipRangeCount = 0) const;

    /**
     * Get read-only access to the mesh data map.
     * Used by TriangleHighlighter to access geometry data.
     */
    const std::unordered_map<utils::Entity, MeshData, utils::Entity::Hasher>& getMeshes() const {
        return mMeshes;
    }

    /**
     * Clear all registered meshes.
     */
    void clear();

    /**
     * Log the total memory usage of indices and expandedIndices for all registered entities.
     * Useful for debugging memory consumption after asset loading.
     */
    void logMemoryUsage() const;

private:
    // Build BVH for a mesh
    static void buildBVH(MeshData& meshData);

    // Compute ray origin and direction in world space from screen coordinates
    // Returns true if successful, false otherwise
    static bool computeScreenRay(View* view, math::int2 position,
                                 math::float3& outOrigin, math::float3& outDirection);

    // Queued during node traversal (AssetLoader); consumed by commitRegistrations().
    // Only populated when enabled == true.
    std::vector<std::pair<utils::Entity, const cgltf_mesh*>> pendingRegistrations;

    // Entity to mesh data mapping
    std::unordered_map<utils::Entity, MeshData, utils::Entity::Hasher> mMeshes;
};

} // namespace filament::gltfio

#endif // GLTFIO_PICKING_REGISTRY_H


