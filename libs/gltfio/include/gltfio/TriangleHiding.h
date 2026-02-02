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

#ifndef GLTFIO_TRIANGLE_HIDING_H
#define GLTFIO_TRIANGLE_HIDING_H

#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/VertexBuffer.h>
#include <gltfio/PickingRegistry.h>

#include <utils/Entity.h>

#include <cstdint>
#include <set>
#include <unordered_map>

namespace filament::gltfio {

class FilamentAsset; // forward declaration

/**
 * TriangleHider manages hiding triangles by creating modified index buffers.
 *
 * This utility allows hiding individual triangles from a renderable entity by:
 * - Creating a vertex buffer with only positions (material handles appearance)
 * - Creating a modified index buffer that excludes hidden triangles
 * - Updating the renderable's geometry with setGeometryAt()
 *
 * Usage:
 * - Call hideTriangle() to hide a single triangle
 * - Call hideTriangles() to hide multiple triangles from the same entity
 * - Call restore() to restore original geometry for all entities
 */
class TriangleHider {
public:
    explicit TriangleHider(filament::Engine* engine);
    ~TriangleHider();

    /**
     * Hide a single triangle from the entity.
     *
     * @param entity The entity to hide triangle from
     * @param triangleIndex The triangle index to hide (from picking)
     * @param asset The FilamentAsset to access mesh cache for correct expanded indices
     * @return true if triangle was hidden successfully
     */
    bool hideTriangle(utils::Entity entity, uint32_t triangleIndex, FilamentAsset* asset);

    /**
     * Hide multiple triangles from an entity.
     *
     * @param entity The entity to hide triangles from
     * @param triangleIndices Set of triangle indices to hide
     * @param asset The FilamentAsset to access mesh cache
     * @return true if triangles were hidden successfully
     */
    bool hideTriangles(utils::Entity entity, const std::set<uint32_t>& triangleIndices,
                      FilamentAsset* asset);

    // Hide a triangle without relying on FilamentAsset mesh cache, by rebuilding VB/IB.
    // MeshData should come from PickingRegistry and includes local positions and original indices.
    bool hideTriangleWithoutCache(utils::Entity entity, uint32_t triangleIndex, const MeshData* meshData);

    // NEW: Hide all triangles that reference vertices in [startVertex, endVertex] inclusive,
    // without relying on FilamentAsset mesh cache.
    bool hideVerticesInRangeWithoutCache(utils::Entity entity, uint32_t startVertex, uint32_t endVertex,
                                         const MeshData* meshData);

private:
    struct HiddenTriangleInfo {
        utils::Entity entity;
        std::set<uint32_t> hiddenTriangleIndices;
        filament::IndexBuffer* modifiedIndexBuffer = nullptr;
        size_t originalIndexCount = 0; // index count before any hides
    };

    bool createHiddenTriangleInfo(utils::Entity entity, uint32_t triangleIndex, FilamentAsset* asset);
    void updateIndexBuffer(HiddenTriangleInfo& info, FilamentAsset* asset);
    filament::IndexBuffer* createIndexBuffer(const uint32_t* indices, size_t count);
    filament::VertexBuffer* createUbershaderCompatibleVB(size_t vertexCount,
                                                         const filament::math::float3* positions) const;

    filament::Engine* mEngine;
    std::unordered_map<uint32_t, HiddenTriangleInfo> mHiddenTriangles;
};

} // namespace filament::gltfio

#endif // GLTFIO_TRIANGLE_HIDING_H

