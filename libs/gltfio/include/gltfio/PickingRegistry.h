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

#ifndef GLTFIO_PICKING_REGISTRY_H
#define GLTFIO_PICKING_REGISTRY_H

#include <filament/Box.h>
#include <math/vec3.h>
#include <math/mat4.h>
#include <utils/Entity.h>
#include <utils/compiler.h>

#include <unordered_map>

#include "Picking.h"

namespace filament::gltfio {

using Entity = utils::Entity;
using mat4f = math::mat4f;
using float3 = math::float3;

class UTILS_PUBLIC PickingRegistry {
public:
    void registerMesh(Entity e, MeshData&& mesh);
    [[nodiscard]] const MeshData* getMesh(Entity e) const;
    void updateTransform(Entity e, const mat4f& world);

    struct Hit { Entity entity; int triangle; float distance; float3 bary; };
    [[nodiscard]] Hit pick(const float3& rayOrigin, const float3& rayDir) const;
    [[nodiscard]] Hit pick(std::pair<math::float3, math::float3> *ray) const;
    [[nodiscard]] Hit * pick(View *view, const int2 &position, FilamentAsset *asset) const;
    [[nodiscard]] Hit pickSkippingIndexRange(const float3& rayOrigin,
                                            const float3& rayDir,
                                            uint32_t startIdx,
                                            uint32_t endIdx) const;

    struct SceneItem { Entity e; Aabb worldBounds; };
private:
    void buildBVHIfNeeded(Entity e);
    MeshData* getMeshMutable(Entity e);

    std::unordered_map<Entity, MeshData, Entity::Hasher> mMeshes;
    std::unordered_map<Entity, mat4f, Entity::Hasher> mWorldTransforms;
};
} // namespace filament::gltfio

#endif // GLTFIO_PICKING_REGISTRY_H
