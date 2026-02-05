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

#include <utils/Entity.h>
#include <math/vec3.h>

#include <vector>
#include <unordered_map>
#include <memory>

// Forward declaration of cgltf types
struct cgltf_mesh;

// Forward declaration of tinybvh BVH
namespace tinybvh {
    class BVH;
}

namespace filament::gltfio {

/**
 * MeshData stores the geometry data and BVH for fast ray-triangle intersection.
 */
struct MeshData {
    std::vector<math::float3> positions;       // Vertex positions in local space
    std::vector<uint32_t> indices;             // Triangle indices (3 per triangle)
    std::unique_ptr<tinybvh::BVH> bvh;         // BVH for accelerated ray tracing

    MeshData() = default;
    MeshData(MeshData&&) noexcept = default;
    MeshData& operator=(MeshData&&) noexcept = default;

    // Disable copy to avoid expensive copies
    MeshData(const MeshData&) = delete;
    MeshData& operator=(const MeshData&) = delete;
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
     * Unregister a mesh associated with an entity.
     *
     * @param entity The entity whose mesh data should be removed
     */
    void unregisterMesh(utils::Entity entity);

    /**
     * Get the mesh data for a specific entity.
     *
     * @param entity The entity to query
     * @return Pointer to MeshData or nullptr if entity not registered
     */
    const MeshData* getMeshData(utils::Entity entity) const;

    /**
     * Check if an entity has registered mesh data.
     *
     * @param entity The entity to check
     * @return true if entity has mesh data, false otherwise
     */
    bool hasMesh(utils::Entity entity) const;

    /**
     * Get the number of registered meshes.
     *
     * @return Number of registered entities
     */
    size_t getMeshCount() const;

    /**
     * Clear all registered meshes.
     */
    void clear();

private:
    // Build BVH for a mesh
    void buildBVH(MeshData& meshData);

    // Entity to mesh data mapping
    std::unordered_map<utils::Entity, MeshData, utils::Entity::Hasher> mMeshes;
};

} // namespace filament::gltfio

#endif // GLTFIO_PICKING_REGISTRY_H


