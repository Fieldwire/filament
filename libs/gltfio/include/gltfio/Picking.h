/*
 * Copyright (C) 2025 The Android Open Source Project
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

#ifndef GLTFIO_PICKING_H
#define GLTFIO_PICKING_H

#include <filament/Box.h>            // for filament::Aabb
#include <math/vec3.h>               // for filament::math::float3
#include <math/mat4.h>               // for filament::math::mat4f
#include <utils/Entity.h>            // for utils::Entity
#include <utils/compiler.h>          // for UTILS_PUBLIC
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace filament::gltfio {

// Use the same alias pattern as other gltfio headers.
using Entity = utils::Entity;

struct MeshTriangle {
    uint32_t i0, i1, i2;
};

struct MeshBVHNode {
    filament::math::float3 min;
    filament::math::float3 max;
    uint32_t left;   // child index or start of triangle range
    uint32_t right;  // child index or count of triangles if leaf
    bool leaf;
};

struct MeshData {
    std::vector<filament::math::float3> positions;   // local-space positions
    std::vector<uint32_t> indices;                   // 3 * triangleCount (triangle list)
    std::vector<MeshBVHNode> bvh;                    // empty until built
    std::vector<uint32_t> leafTris;                  // triangle indices order for leaves
    filament::Aabb localBounds;                      // un-transformed local bounds
    bool bvhBuilt = false;                           // BVH build flag
};

class UTILS_PUBLIC PickingRegistry {
public:
    /** Register a mesh's CPU data for picking. */
    void registerMesh(Entity e, MeshData&& mesh);
    /** Get immutable mesh data (or nullptr if not registered). */
    const MeshData* getMesh(Entity e) const;
    /** Get mutable mesh data (or nullptr if not registered). */
    MeshData* getMeshMutable(Entity e);
    /** Build the BVH lazily if not yet built. */
    void buildBVHIfNeeded(Entity e);
    /** Placeholder for transform update caching (not yet implemented). */
    void updateTransform(Entity e, const filament::math::mat4f& world); // optional cache placeholder

    struct Hit { Entity entity; int triangle; float distance; filament::math::float3 bary; };
    /** Ray pick against all registered meshes (returns closest hit or entity==0 if none). */
    Hit pick(const filament::math::float3& rayOrigin, const filament::math::float3& rayDir) const;

    // Scene broadphase item (optional usage)
    struct SceneItem { Entity e; filament::Aabb worldBounds; };
private:
    std::unordered_map<Entity, MeshData, Entity::Hasher> mMeshes;
    std::unordered_map<Entity, filament::math::mat4f, Entity::Hasher> mWorldTransforms; // cached world transforms (optional)
    // Optionally maintain a flat vector of SceneItem for faster iteration.
};

} // namespace filament::gltfio


#endif // GLTFIO_PICKING_H
