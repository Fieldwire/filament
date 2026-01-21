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

#include <gltfio/TriangleHiding.h>
#include <cstdio>

#include "math/norm.h"

#include <gltfio/Picking.h>
#include <gltfio/FilamentAsset.h>
#include "FFilamentAsset.h"

#include <filament/RenderableManager.h>

#include <utils/Log.h>

namespace filament::gltfio {

TriangleHider::TriangleHider(filament::Engine* engine) : mEngine(engine) {
}

TriangleHider::~TriangleHider() {
    //restoreAll();
}

// Helper function to find the primitive in mesh cache that matches the entity's VertexBuffer
static const gltfio::Primitive* findPrimitiveInCache(const gltfio::MeshCache& meshCache,
                                                      filament::VertexBuffer* vb) {
    for (const auto& mesh : meshCache) {
        for (const auto& primitive : mesh) {
            if (primitive.vertices == vb) {
                return &primitive;
            }
        }
    }
    return nullptr;
}

bool TriangleHider::hideTriangle(utils::Entity entity, uint32_t triangleIndex,
                                FilamentAsset* asset) {
    if (!asset) {
        printf("ERROR: FilamentAsset is null\n");
        return false;
    }

    auto& rcm = mEngine->getRenderableManager();
    auto renderableInst = rcm.getInstance(entity);
    if (!renderableInst) {
        return false;
    }

    uint32_t entityId = entity.getId();
    auto it = mHiddenTriangles.find(entityId);

    if (it == mHiddenTriangles.end()) {
        // First time hiding a triangle for this entity
        return createHiddenTriangleInfo(entity, triangleIndex, asset);
    } else {
        // Already have hidden triangles - hide another one
        auto& info = it->second;

        if (info.hiddenTriangleIndices.find(triangleIndex) != info.hiddenTriangleIndices.end()) {
            // Already hidden
            return false;
        }

        info.hiddenTriangleIndices.insert(triangleIndex);
        updateIndexBuffer(info, asset);
        return true;
    }
}

bool TriangleHider::hideTriangles(utils::Entity entity, const std::set<uint32_t>& triangleIndices,
                                 FilamentAsset* asset) {
    bool success = true;
    for (uint32_t triangleIdx : triangleIndices) {
        if (!hideTriangle(entity, triangleIdx, asset)) {
            success = false;
        }
    }
    return success;
}

bool TriangleHider::createHiddenTriangleInfo(utils::Entity entity, uint32_t triangleIndex,
                                            FilamentAsset* asset) {
    auto& rcm = mEngine->getRenderableManager();
    auto renderableInst = rcm.getInstance(entity);
    if (!renderableInst) {
        return false;
    }

    // Get the VertexBuffer from the renderable
    auto* vb = rcm.getVertexBuffer(renderableInst, 0);
    if (!vb) {
        printf("ERROR: No vertex buffer found for entity %u\n", entity.getId());
        return false;
    }

    // Find the matching primitive in mesh cache
    auto* fAsset = static_cast<FFilamentAsset*>(asset);
    const auto& meshCache = fAsset->getMeshCache();
    const auto* primitive = findPrimitiveInCache(meshCache, vb);

    if (!primitive || !primitive->indices) {
        printf("ERROR: Could not find primitive in mesh cache for entity %u\n", entity.getId());
        return false;
    }

    // Use the mesh cache IndexBuffer as the source of truth for index count.
    // Note: we cannot read back index data from the GPU, but for our expanded meshes the indices
    // are sequential (0..N-1). The important part is that the count comes from the cached IB.
    const size_t originalIndexCount = primitive->indices->getIndexCount();
    printf("[TH] meshCache primitive VB=%p IB=%p indexCount=%zu\n", (void*)primitive->vertices, (void*)primitive->indices, originalIndexCount);
    if (originalIndexCount % 3 != 0) {
        printf("ERROR: Index count is not divisible by 3 (indexCount=%zu) for entity %u\n",
               originalIndexCount, entity.getId());
        return false;
    }

    // Create new index buffer excluding the hidden triangle
    size_t newIndexCount = originalIndexCount - 3;
    auto* newIndices = new uint32_t[newIndexCount];

    size_t writeIdx = 0;
    for (size_t i = 0; i < originalIndexCount; i += 3) {
        const uint32_t tri = uint32_t(i / 3);
        if (tri == triangleIndex) {
            continue;
        }
        // Sequential indices: 0..originalIndexCount-1
        newIndices[writeIdx++] = (uint32_t)i;
        newIndices[writeIdx++] = (uint32_t)(i + 1);
        newIndices[writeIdx++] = (uint32_t)(i + 2);
    }

    auto* ib = createIndexBuffer(newIndices, newIndexCount);
    delete[] newIndices;

    // Update geometry - reuse original VertexBuffer, use new IndexBuffer
    rcm.setGeometryAt(renderableInst, 0,
                     RenderableManager::PrimitiveType::TRIANGLES,
                     vb, ib, 0, newIndexCount);

    // Store info
    HiddenTriangleInfo info;
    info.entity = entity;
    info.originalIndexCount = originalIndexCount;
    info.hiddenTriangleIndices.insert(triangleIndex);
    info.modifiedIndexBuffer = ib;
    mHiddenTriangles[entity.getId()] = info;

    printf("Hidden triangle %u. New index count: %zu\n", triangleIndex, newIndexCount);
    printf("[TH] setGeometryAt: newIndexCount=%zu VB=%p IB=%p\n", newIndexCount, (void*)vb, (void*)ib);
    return true;
}

void TriangleHider::updateIndexBuffer(HiddenTriangleInfo& info, FilamentAsset* asset) {
    auto& rcm = mEngine->getRenderableManager();
    auto renderableInst = rcm.getInstance(info.entity);
    if (!renderableInst) {
        return;
    }

    // Fetch current VB from RCM each time (don't store in info to avoid stale pointers)
    auto* originalVB = rcm.getVertexBuffer(renderableInst, 0);
    if (!originalVB || info.originalIndexCount == 0) {
        printf("ERROR: No vertex buffer or index count stored.\n");
        return;
    }

    printf("Updating index buffer for entity %u (originalIndexCount=%zu)\n",
           info.entity.getId(), info.originalIndexCount);
    printf("[TH] updateIndexBuffer hiddenCount=%zu originalIndexCount=%zu\n",
           info.hiddenTriangleIndices.size(), info.originalIndexCount);

    // Rebuild index buffer excluding all hidden triangles
    // We cannot read index data back from the GPU. For our expanded meshes we generate
    // sequential indices (0..originalIndexCount-1) and only adjust the count.
    size_t originalIndexCount = info.originalIndexCount;
    size_t newIndexCount = originalIndexCount - (info.hiddenTriangleIndices.size() * 3);
    auto* newIndices = new uint32_t[newIndexCount];

    size_t writeIdx = 0;
    for (size_t i = 0; i < originalIndexCount; i += 3) {
        uint32_t triangleIdx = i / 3;
        if (info.hiddenTriangleIndices.find(triangleIdx) == info.hiddenTriangleIndices.end()) {
            newIndices[writeIdx++] = i;
            newIndices[writeIdx++] = i + 1;
            newIndices[writeIdx++] = i + 2;
        }
    }

    // Destroy old index buffer
    if (info.modifiedIndexBuffer) {
        mEngine->destroy(info.modifiedIndexBuffer);
    }

    // Create new index buffer
    auto* ib = createIndexBuffer(newIndices, newIndexCount);
    delete[] newIndices;

    info.modifiedIndexBuffer = ib;

    // Update geometry
    rcm.setGeometryAt(renderableInst, 0,
                     RenderableManager::PrimitiveType::TRIANGLES,
                     originalVB, ib, 0, newIndexCount);

    printf("Hidden %zu triangles. New index count: %zu\n",
           info.hiddenTriangleIndices.size(), newIndexCount);
    printf("[TH] updateIndexBuffer setGeometryAt VB=%p IB=%p indexCount=%zu\n",
           (void*)originalVB, (void*)ib, newIndexCount);
}

// Ensure function declaration is properly formed (no macro confusion)
bool TriangleHider::hideTriangleWithoutCache(utils::Entity entity, uint32_t triangleIndex,
                                             const MeshData* meshData) {
    if (!meshData || meshData->positions.empty() || meshData->indices.empty()) {
        printf("ERROR: MeshData is empty or invalid.\n");
        return false;
    }

    auto& rcm = mEngine->getRenderableManager();
    auto renderableInst = rcm.getInstance(entity);
    if (!renderableInst) {
        printf("ERROR: Entity has no renderable instance.\n");
        return false;
    }

    const size_t uniqueVertexCount = meshData->positions.size();
    const size_t originalIndexCount = meshData->indices.size();
    if (originalIndexCount % 3 != 0) {
        printf("ERROR: MeshData indices not divisible by 3 (count=%zu).\n", originalIndexCount);
        return false;
    }

    // Expand to per-index vertex stream to mirror AssetLoaderExtended: vertexCount == indexCount
    // expandedPositions[i] = positions[ indices[i] ]
    auto* expandedPositions = new filament::math::float3[originalIndexCount];
    for (size_t i = 0; i < originalIndexCount; ++i) {
        uint32_t idx = meshData->indices[i];
        if (idx >= uniqueVertexCount) {
            printf("ERROR: MeshData index %u out of range (uniqueVertexCount=%zu).\n", idx, uniqueVertexCount);
            delete[] expandedPositions;
            return false;
        }
        expandedPositions[i] = meshData->positions[idx];
    }

    // Build VB compatible with UberShader expectations using expanded positions.
    auto* vb = createUbershaderCompatibleVB(originalIndexCount, expandedPositions);
    delete[] expandedPositions;

    if (!vb) {
        printf("ERROR: Failed to create UberShader-compatible VB.\n");
        return false;
    }

    // Build new sequential IB excluding the triangleIndex.
    size_t newIndexCount = originalIndexCount - 3;
    auto* newIndices = new uint32_t[newIndexCount];

    size_t writeIdx = 0;
    for (size_t i = 0; i < originalIndexCount; i += 3) {
        uint32_t tri = uint32_t(i / 3);
        if (tri == triangleIndex) continue;
        newIndices[writeIdx++] = (uint32_t)i;
        newIndices[writeIdx++] = (uint32_t)(i + 1);
        newIndices[writeIdx++] = (uint32_t)(i + 2);
    }

    auto* ib = createIndexBuffer(newIndices, newIndexCount);
    delete[] newIndices;

    // Update geometry: use the newly built VB/IB.
    rcm.setGeometryAt(renderableInst, 0,
                      RenderableManager::PrimitiveType::TRIANGLES,
                      vb, ib, 0, newIndexCount);

    // Track info so subsequent hides can be processed (using originalIndexCount from MeshData).
    HiddenTriangleInfo info;
    info.entity = entity;
    info.originalIndexCount = originalIndexCount;
    info.hiddenTriangleIndices.insert(triangleIndex);
    info.modifiedIndexBuffer = ib;
    mHiddenTriangles[entity.getId()] = info;

    printf("[TH] hideTriangleWithoutCache: expandedVertexCount=%zu newIndexCount=%zu\n",
           originalIndexCount, newIndexCount);

    return true;
}

bool TriangleHider::hideVerticesInRangeWithoutCache(utils::Entity entity, uint32_t startVertex,
                                                     uint32_t endVertex, const MeshData* meshData) {
    // Interpret startVertex/endVertex as index buffer positions (0..indices.size()-1),
    // not unique vertex ids. Skip all triangles that overlap this index range.
    if (!meshData || meshData->positions.empty() || meshData->indices.empty()) {
        printf("ERROR: MeshData is empty or invalid.\n");
        return false;
    }
    if (startVertex > endVertex) {
        printf("ERROR: startIndex > endIndex (%u > %u).\n", startVertex, endVertex);
        return false;
    }

    auto& rcm = mEngine->getRenderableManager();
    auto renderableInst = rcm.getInstance(entity);
    if (!renderableInst) {
        printf("ERROR: Entity has no renderable instance.\n");
        return false;
    }

    const size_t uniqueVertexCount = meshData->positions.size();
    const size_t originalIndexCount = meshData->indices.size();
    if (originalIndexCount % 3 != 0) {
        printf("ERROR: MeshData indices not divisible by 3 (count=%zu).\n", originalIndexCount);
        return false;
    }

    if (endVertex >= originalIndexCount) {
        printf("ERROR: endIndex out of range (%u >= %zu).\n", endVertex, originalIndexCount);
        return false;
    }

    // Expand positions to match per-index vertex layout (index stream as vertex stream).
    auto* expandedPositions = new filament::math::float3[originalIndexCount];
    for (size_t i = 0; i < originalIndexCount; ++i) {
        uint32_t idx = meshData->indices[i];
        if (idx >= uniqueVertexCount) {
            printf("ERROR: MeshData index %u out of range (uniqueVertexCount=%zu).\n", idx, uniqueVertexCount);
            delete[] expandedPositions;
            return false;
        }
        expandedPositions[i] = meshData->positions[idx];
    }

    // Build VB compatible with UberShader expectations using expanded positions.
    auto* vb = createUbershaderCompatibleVB(originalIndexCount, expandedPositions);
    delete[] expandedPositions;
    if (!vb) {
        printf("ERROR: Failed to create UberShader-compatible VB.\n");
        return false;
    }

    // Build new sequential IB excluding any triangle whose index positions overlap [startVertex, endVertex].
    std::vector<uint32_t> newIndices;
    newIndices.reserve(originalIndexCount);

    for (size_t i = 0; i < originalIndexCount; i += 3) {
        bool i0InRange = (i + 0 >= startVertex && i + 0 <= endVertex);
        bool i1InRange = (i + 1 >= startVertex && i + 1 <= endVertex);
        bool i2InRange = (i + 2 >= startVertex && i + 2 <= endVertex);
        if (i0InRange || i1InRange || i2InRange) {
            // Skip this triangle entirely if any of its index positions fall in the range.
            continue;
        }
        // Keep this triangle with sequential indices referencing expanded VB
        newIndices.push_back((uint32_t) i);
        newIndices.push_back((uint32_t) (i + 1));
        newIndices.push_back((uint32_t) (i + 2));
    }

    const size_t newIndexCount = newIndices.size();
    auto* ib = createIndexBuffer(newIndices.data(), newIndexCount);

    // Update geometry: use the newly built VB/IB.
    rcm.setGeometryAt(renderableInst, 0,
                      RenderableManager::PrimitiveType::TRIANGLES,
                      vb, ib, 0, newIndexCount);

    // Track info for this entity so subsequent operations can be aware of original counts
    HiddenTriangleInfo info;
    info.entity = entity;
    info.originalIndexCount = originalIndexCount;
    info.modifiedIndexBuffer = ib;
    mHiddenTriangles[entity.getId()] = info;

    printf("[TH] hideVerticesInRangeWithoutCache (index-range): start=%u end=%u keptIndices=%zu (from %zu)\n",
           startVertex, endVertex, newIndexCount, originalIndexCount);

    return true;
}

filament::VertexBuffer* TriangleHider::createUbershaderCompatibleVB(size_t vertexCount,
        const filament::math::float3* positions) const {
    using namespace filament;
    using namespace filament::math;

    auto vbBuilder = VertexBuffer::Builder().vertexCount(vertexCount).bufferCount(5);

    // POSITION (slot 0)
    vbBuilder.attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT3);

    // UV slots must mirror AssetLoaderExtended: UV0 at slot 2, UV1 at slot 1
    vbBuilder.attribute(VertexAttribute::UV0, 2, VertexBuffer::AttributeType::USHORT2);
    vbBuilder.normalized(VertexAttribute::UV0);
    vbBuilder.attribute(VertexAttribute::UV1, 1, VertexBuffer::AttributeType::USHORT2);
    vbBuilder.normalized(VertexAttribute::UV1);

    // COLOR (slot 3) white
    vbBuilder.attribute(VertexAttribute::COLOR, 3, VertexBuffer::AttributeType::FLOAT4);

    // TANGENTS (slot 4) SHORT4 normalized (packed quaternion)
    vbBuilder.attribute(VertexAttribute::TANGENTS, 4, VertexBuffer::AttributeType::SHORT4);
    vbBuilder.normalized(VertexAttribute::TANGENTS);

    auto* vb = vbBuilder.build(*mEngine);

    // POSITION data
    auto* posCopy = new float3[vertexCount];
    memcpy(posCopy, positions, sizeof(float3) * vertexCount);
    vb->setBufferAt(*mEngine, 0,
        VertexBuffer::BufferDescriptor(posCopy, sizeof(float3) * vertexCount,
            [](void* buffer, size_t, void*) { delete[] static_cast<float3*>(buffer); }));

    // UV1 dummy (slot 1)
    auto* uv1 = new ushort2[vertexCount];
    memset(uv1, 0xff, sizeof(ushort2) * vertexCount);
    vb->setBufferAt(*mEngine, 1,
        VertexBuffer::BufferDescriptor(uv1, sizeof(ushort2) * vertexCount,
            [](void* buffer, size_t, void*) { delete[] static_cast<ushort2*>(buffer); }));

    // UV0 dummy (slot 2)
    auto* uv0 = new ushort2[vertexCount];
    memset(uv0, 0xff, sizeof(ushort2) * vertexCount);
    vb->setBufferAt(*mEngine, 2,
        VertexBuffer::BufferDescriptor(uv0, sizeof(ushort2) * vertexCount,
            [](void* buffer, size_t, void*) { delete[] static_cast<ushort2*>(buffer); }));

    // COLOR white (slot 3)
    auto* color = new float4[vertexCount];
    for (size_t i = 0; i < vertexCount; ++i) {
        color[i] = float4{1.0f, 1.0f, 1.0f, 1.0f};
    }
    vb->setBufferAt(*mEngine, 3,
        VertexBuffer::BufferDescriptor(color, sizeof(float4) * vertexCount,
            [](void* buffer, size_t, void*) { delete[] static_cast<float4*>(buffer); }));

    // TANGENTS per triangle (slot 4): compute face normal/tangent/bitangent and pack
    auto* tbn = new short4[vertexCount];
    if (vertexCount % 3 == 0) {
        for (size_t i = 0; i < vertexCount; i += 3) {
            float3 const p0 = positions[i + 0];
            float3 const p1 = positions[i + 1];
            float3 const p2 = positions[i + 2];
            float3 const e0 = normalize(p1 - p0);
            float3 const n  = normalize(cross(p1 - p0, p2 - p0));
            // Ensure a valid basis even if e0 is near parallel to n
            float3 const t  = length(e0) > 0 ? e0 : float3{1,0,0};
            float3 const b  = normalize(cross(n, t));
            mat3f frame{ t, b, n };
            // Pack quaternion from frame and then into snorm16 short4
            short4 const packed = packSnorm16(mat3f::packTangentFrame(frame).xyzw);
            tbn[i + 0] = packed;
            tbn[i + 1] = packed;
            tbn[i + 2] = packed;
        }
    } else {
        // Fallback: default TBN
        short4 defaultTbn = short4{32767, 0, 0, 0};
        for (size_t i = 0; i < vertexCount; ++i) {
            tbn[i] = defaultTbn;
        }
    }
    vb->setBufferAt(*mEngine, 4,
        VertexBuffer::BufferDescriptor(tbn, sizeof(short4) * vertexCount,
            [](void* buffer, size_t, void*) { delete[] static_cast<short4*>(buffer); }));

    printf("[TH] createUbershaderCompatibleVB: vertexCount=%zu (UV0->slot2, UV1->slot1, per-triangle TBN)\n", vertexCount);

    return vb;
}

filament::IndexBuffer* TriangleHider::createIndexBuffer(const uint32_t* indices, size_t count) {
    using namespace filament;
    auto* ib = IndexBuffer::Builder()
        .indexCount(count)
        .bufferType(IndexBuffer::IndexType::UINT)
        .build(*mEngine);

    auto* indicesCopy = new uint32_t[count];
    memcpy(indicesCopy, indices, count * sizeof(uint32_t));

    ib->setBuffer(*mEngine,
                 IndexBuffer::BufferDescriptor(
                     indicesCopy, count * sizeof(uint32_t),
                     [](void* buffer, size_t, void*) { delete[] static_cast<uint32_t*>(buffer); }
                 ));

    return ib;
}

} // namespace filament::gltfio
