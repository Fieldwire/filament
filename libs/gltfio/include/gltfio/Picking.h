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

#include <filament/Box.h>
#include <math/vec3.h>
#include <math/mat4.h>
#include <utils/Entity.h>
#include <utils/compiler.h>

#include <vector>
#include <unordered_map>
#include <cstdint>

#include <cgltf.h>

namespace filament::gltfio {

// Use the same alias pattern as other gltfio headers.
using Entity = utils::Entity;

struct MeshTriangle { uint32_t i0, i1, i2; };

struct MeshBVHNode {
    filament::math::float3 min;
    filament::math::float3 max;
    uint32_t left;   // child index OR start of triangle range if leaf
    uint32_t right;  // child index OR triangle count if leaf
    bool leaf;
};

struct MeshData {
    std::vector<filament::math::float3> positions;   // local-space vertex positions
    std::vector<uint32_t> indices;                   // triangle list (3 * triCount)
    std::vector<MeshBVHNode> bvh;                    // BVH nodes (empty until built)
    std::vector<uint32_t> leafTris;                  // triangle ordinals for leaves
    filament::Aabb localBounds;                      // un-transformed bounds
    bool bvhBuilt = false;                           // BVH build flag
};

// Build CPU mesh data for picking from a cgltf mesh (triangles only). localBounds left empty.
MeshData buildMeshDataForPicking(const cgltf_mesh* mesh);

class UTILS_PUBLIC PickingRegistry {
public:
    void registerMesh(Entity e, MeshData&& mesh);
    [[nodiscard]] const MeshData* getMesh(Entity e) const;
    void updateTransform(Entity e, const filament::math::mat4f& world);

    struct Hit { Entity entity; int triangle; float distance; filament::math::float3 bary; };
    [[nodiscard]] Hit pick(const filament::math::float3& rayOrigin, const filament::math::float3& rayDir) const;

    struct SceneItem { Entity e; filament::Aabb worldBounds; };
private:
    void buildBVHIfNeeded(Entity e);
    MeshData* getMeshMutable(Entity e);

    std::unordered_map<Entity, MeshData, Entity::Hasher> mMeshes;
    std::unordered_map<Entity, filament::math::mat4f, Entity::Hasher> mWorldTransforms;
};

} // namespace filament::gltfio


#endif // GLTFIO_PICKING_H
