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
    std::vector<tinybvh::bvhvec4> bvhVertices;  // per-vertex, shou

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
    math::float3 bary;              // Barycentric coordinates of the hit point

    bool hasHit() const { return triangleIndex >= 0; }
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
     * Perform ray-triangle intersection test against a specific entity's mesh.
     * Computes ray from screen coordinates and optionally skips triangles in the specified index ranges.
     *
     * @param view The View to use for screen-to-ray conversion
     * @param entity The entity to test against
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param sortedSkipRanges Flat array of skip ranges: [start0, end0, start1, end1, ...], can be nullptr.
     *                 Each range is inclusive and specifies triangle index ranges to skip during intersection.
     *                 Note: triangle indices refer to the position in the mesh's index buffer (not the primitive index).
     * @param skipRangeCount Number of range PAIRS in skipRanges array (NOT total array length)
     * @return Hit information (triangleIndex = -1 if no hit)
     */
    PickingHit pick(View& view,
                    TransformManager& tcm,
                    utils::Entity entity,
                    int screenX,
                    int screenY,
                    const uint32_t* sortedSkipRanges = nullptr,
                    size_t skipRangeCount = 0) const;

    /**
     * Clear all registered meshes.
     */
    void clear();

private:
    // Build BVH for a mesh
    static void buildBVH(MeshData& meshData);

    // Compute ray origin and direction in world space from screen coordinates
    static std::pair<math::float3, math::float3> *computeScreenRay(View* view, math::int2 position);

    // Entity to mesh data mapping
    std::unordered_map<utils::Entity, MeshData, utils::Entity::Hasher> mMeshes;
};

} // namespace filament::gltfio

#endif // GLTFIO_PICKING_REGISTRY_H


