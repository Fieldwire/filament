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

#include <gltfio/PickingRegistry.h>

// Provide aligned_alloc for older Android API levels (<28)
// aligned_alloc was added in API level 28, but tinybvh requires it for SIMD optimizations.
#if defined(__ANDROID__) && (__ANDROID_API__ < 28)
extern "C" void* aligned_alloc(size_t alignment, size_t size) {
    size_t rem = size % alignment;
    if (rem != 0) size += (alignment - rem);
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0) return nullptr;
    return ptr;
}
#endif

// Suppress warnings from tinybvh about implicit conversions in ray construction
// and unguarded availability for aligned_alloc on older Android
// This is a localized suppression around the tinybvh include, popped right after compiling the header,
// so it won't affect the rest of the codebase
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-const-int-float-conversion"
#if defined(__ANDROID__)
#pragma clang diagnostic ignored "-Wunguarded-availability"
#endif
#endif

// TinyBVH - Fast BVH-based ray tracing
// Source: https://github.com/jbikker/tinybvh (commit 4b5b649)
#define TINYBVH_IMPLEMENTATION
#include "../../third_party/tiny-bvh/tiny_bvh.h"

// Pop the diagnostic state to restore warnings after including tinybvh
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "filament/Camera.h"
#include "filament/TransformManager.h"
#include "filament/View.h"
#include "filament/Viewport.h"
#include "gltfio/Animator.h"

#include <cgltf.h>
#include <utils/Log.h>

#include <algorithm>

using namespace filament::math;
using namespace utils;

namespace filament::gltfio {

// ============================================================================
// Constructor / Destructor
// ============================================================================

PickingRegistry::PickingRegistry() = default;

// Destructor must be defined in .cpp where MeshData is complete
// This allows std::unique_ptr<tinybvh::BVH> to properly destroy
PickingRegistry::~PickingRegistry() = default;

// Move operations must be defined where MeshData is complete
PickingRegistry::PickingRegistry(PickingRegistry&&) noexcept = default;
PickingRegistry& PickingRegistry::operator=(PickingRegistry&&) noexcept = default;

// ============================================================================
// Mesh Registration
// ============================================================================

bool PickingRegistry::commitRegistrations(
    const cgltf_mesh *gltfMeshes,
    const size_t meshCacheSize,
    const std::function<size_t(size_t)> &getPrimsSize,
    const std::function<std::vector<uint32_t>&(size_t, size_t)> &getExpandedIndices
) {
    if (pendingRegistrations.empty()) {
        return true;
    }

    bool allSucceeded = true;

    for (const auto& [entity, mesh] : pendingRegistrations) {
        if (!registerMesh(entity, mesh)) {
            allSucceeded = false;
            continue;
        }

        const cgltf_size meshIndex = mesh - gltfMeshes;
        if (meshIndex < meshCacheSize) {
            const auto primsSize = getPrimsSize(meshIndex);

            if (primsSize > 1) {
                slog.w << "Mesh '" << (mesh->name ? mesh->name : "<unnamed>")
                       << "' has " << primsSize << " primitives. "
                       << "ExpandedIndices registration only supports single-primitive meshes. "
                       << "Triangle filtering may not work correctly for this mesh." << io::endl;
            }

            if (primsSize == 1) {
                auto& expandedIndices = getExpandedIndices(meshIndex, 0);
                if (!expandedIndices.empty()) {
                    registerExpandedIndices(entity,
                        expandedIndices.data(),
                        expandedIndices.size());
                }
            }
        }
    }

    pendingRegistrations.clear();

    // Free expandedIndices from meshCache now that all entities are registered
    for (size_t i = 0; i < meshCacheSize; i++) {
        const size_t primsSize = getPrimsSize(i);
        for (size_t j = 0; j < primsSize; j++) {
            auto& expandedIndices = getExpandedIndices(i, j);
            expandedIndices.clear();
            expandedIndices.shrink_to_fit();
        }
    }

    logMemoryUsage();
    return allSucceeded;
}

bool PickingRegistry::registerMesh(const Entity entity, const cgltf_mesh* mesh) {
    if (!mesh) {
        slog.w << "PickingRegistry: NULL mesh pointer for entity " << entity.getId() << io::endl;
        return false;
    }

    std::vector<float3> positions;
    std::vector<uint32_t> indices;

    // Iterate through all primitives in the mesh
    for (size_t primIdx = 0; primIdx < mesh->primitives_count; ++primIdx) {
        const cgltf_primitive& primitive = mesh->primitives[primIdx];

        // Only handle triangle primitives
        if (primitive.type != cgltf_primitive_type_triangles) {
            continue;
        }

        // Extract vertex positions
        const cgltf_accessor* posAccessor = nullptr;
        for (size_t attrIdx = 0; attrIdx < primitive.attributes_count; ++attrIdx) {
            if (primitive.attributes[attrIdx].type == cgltf_attribute_type_position) {
                posAccessor = primitive.attributes[attrIdx].data;
                break;
            }
        }

        if (!posAccessor) {
            slog.w << "PickingRegistry: No position attribute in primitive " << primIdx << io::endl;
            continue;
        }

        // Read vertex positions
        size_t vertexOffset = positions.size();
        size_t vertexCount = posAccessor->count;
        positions.resize(vertexOffset + vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
            float pos[3];
            cgltf_accessor_read_float(posAccessor, i, pos, 3);
            positions[vertexOffset + i] = float3(pos[0], pos[1], pos[2]);
        }

        // Extract indices
        const cgltf_accessor* indexAccessor = primitive.indices;
        if (indexAccessor) {
            size_t indexCount = indexAccessor->count;
            size_t indexOffset = indices.size();
            indices.resize(indexOffset + indexCount);

            for (size_t i = 0; i < indexCount; ++i) {
                indices[indexOffset + i] = static_cast<uint32_t>(
                    cgltf_accessor_read_index(indexAccessor, i) + vertexOffset
                );
            }
        } else {
            size_t indexOffset = indices.size();
            indices.resize(indexOffset + vertexCount);
            for (size_t i = 0; i < vertexCount; ++i) {
                indices[indexOffset + i] = static_cast<uint32_t>(vertexOffset + i);
            }
        }
    }

    if (positions.empty() || indices.empty()) {
        slog.w << "PickingRegistry: No valid geometry extracted from mesh" << io::endl;
        return false;
    }

    // Create mesh data and build BVH
    MeshData meshData;
    meshData.positions = std::move(positions);
    meshData.indices = std::move(indices);

    // Validate mesh data
    if (meshData.indices.size() % 3 != 0) {
        slog.w << "PickingRegistry: Invalid index count " << meshData.indices.size()
               << " for entity " << entity.getId() << " (must be multiple of 3)" << io::endl;
        return false;
    }

    // Build BVH for fast ray intersection
    buildBVH(meshData);

    // Store in registry
    mMeshes[entity] = std::move(meshData);

    return true;
}

bool PickingRegistry::registerExpandedIndices(const Entity entity,
                                              const uint32_t* expandedIndices, size_t indexCount) {
    auto it = mMeshes.find(entity);
    if (it == mMeshes.end()) {
        slog.w << "PickingRegistry: Cannot register expanded indices for unknown entity " << entity.getId() << io::endl;
        return false;
    }

    MeshData& meshData = it->second;

    // Store the expanded indices that match the renderable's vertex buffer
    // The vertex buffer is owned by the renderable - we don't need to store positions
    // These indices will be returned by nGetMeshIndicesBuffer for use in triangle filtering
    meshData.expandedIndices.assign(expandedIndices, expandedIndices + indexCount);

    return true;
}

// ============================================================================
// BVH Construction
// ============================================================================

void PickingRegistry::buildBVH(MeshData& meshData) {
    const size_t triangleCount = meshData.indices.size() / 3;

    if (triangleCount == 0) {
        slog.w << "PickingRegistry: Cannot build BVH for mesh with 0 triangles" << io::endl;
        return;
    }

    // Create BVH instance
    meshData.bvh = std::make_unique<tinybvh::BVH>();

    // Convert vertex positions to TinyBVH format (bvhvec4 per unique vertex position).
    // We build indexed geometry: one bvhvec4 per vertex in positions array,
    // referenced by the index buffer. TinyBVH stores a pointer to this data, so it must persist!
    meshData.bvhVertices.clear();
    meshData.bvhVertices.reserve(meshData.positions.size());

    for (const auto& p : meshData.positions) {
        meshData.bvhVertices.emplace_back(p.x, p.y, p.z, 0.0f);
    }

    // Build the BVH using indexed geometry (vertex buffer + index buffer)
    meshData.bvh->Build(meshData.bvhVertices.data(), meshData.indices.data(), triangleCount);
}

// ============================================================================
// Ray-Triangle Intersection with Triangle Filtering
// ============================================================================

PickingHit PickingRegistry::pick(View& view,
                                 TransformManager& tcm,
                                 Entity entity,
                                 int screenX,
                                 int screenY,
                                 const uint32_t* sortedTriangleSkipRanges,
                                 size_t skipRangeCount) const {
    PickingHit hit;
    // Initialize hit distance to max so any valid hit will be closer than this
    hit.distance = std::numeric_limits<float>::max();

    // Compute ray from screen coordinates in world space
    float3 worldRayOrigin;
    float3 worldRayDirection;
    if (!computeScreenRay(&view, math::int2(screenX, screenY), worldRayOrigin, worldRayDirection)) {
        return hit; // Failed to compute ray
    }

    // Find the mesh for this entity
    auto it = mMeshes.find(entity);
    if (it == mMeshes.end() || !it->second.bvh) {
        return hit; // No mesh or BVH for this entity
    }

    const MeshData& meshData = it->second;

    // Get entity's transform and convert to local space
    auto instance = tcm.getInstance(entity);
    if (!instance) {
        return hit; // Entity has no transform
    }

    // Get inverse of world transform to convert ray to local space
    mat4f worldTransform = tcm.getWorldTransform(instance);
    mat4f localTransform = inverse(worldTransform);

    // Transform ray to local (model) space
    float3 rayOrigin = (localTransform * float4(worldRayOrigin, 1.0f)).xyz;
    float3 rayDir = normalize((localTransform * float4(worldRayDirection, 0.0f)).xyz);

    // Use custom intersection callback for triangle filtering
    tinybvh::TriangleFilterContext ctx{};
    ctx.skipRanges = sortedTriangleSkipRanges;
    ctx.skipRangeCount = skipRangeCount;

    // Setup tinybvh ray - MUST use constructor to properly initialize rD
    // (reciprocal direction)! The constructor normalizes D and computes rD =
    // 1/D which is required for BVH traversal
    tinybvh::bvhvec3 origin(rayOrigin.x, rayOrigin.y, rayOrigin.z);
    tinybvh::bvhvec3 direction(rayDir.x, rayDir.y, rayDir.z);
    tinybvh::Ray bvhRay(origin, direction, hit.distance);
    bvhRay.hit.auxData = &ctx; // Pass context to callback

    // Intersect with BVH - callback will skip hidden triangles during traversal
    meshData.bvh->Intersect(bvhRay);

    // Check if we found a hit (TinyBVH updates ray.hit.t to actual distance)
    if (bvhRay.hit.t < hit.distance && bvhRay.hit.t > 0) {
        hit.entity = entity;
        hit.triangleIndex = static_cast<int>(bvhRay.hit.prim);
        hit.distance = bvhRay.hit.t;
    }

    return hit;
}

bool PickingRegistry::computeScreenRay(View* view, int2 position,
                                       float3& outOrigin, float3& outDirection) {
    if (!view) return false;

    Camera* cam = &view->getCamera();

    auto sx = position.x;
    auto sy = position.y;

    const Viewport& vp = view->getViewport();
    if (vp.width <= 0 || vp.height <= 0) return false;

    double nx = (double(sx) / double(vp.width)) * 2.0 - 1.0;
    double ny = (double(sy) / double(vp.height)) * 2.0 - 1.0;

    mat4 proj = cam->getProjectionMatrix();
    bool isPerspective = std::abs(proj[3][3]) < 1e-6;

    mat4 invProj = Camera::inverseProjection(proj);
    mat4 viewM = cam->getViewMatrix();
    mat4 invView = inverse(viewM);

    double4 clip{ nx, ny, 0.0, 1.0 }; // near plane
    double4 viewSpace = invProj * clip;

    if (!std::isfinite(viewSpace.w) || std::abs(viewSpace.w) < 1e-12) {
        // This should never happen for near-plane un-projection.
        // Bail out instead of fabricating data.
        return false;
    }

    viewSpace = viewSpace / viewSpace.w;
    auto viewPoint = float3( (float)viewSpace.x, (float)viewSpace.y, (float)viewSpace.z );

    if (isPerspective) {
        float3 dirView = normalize(viewPoint);
        float3 dirWorld = normalize( (invView * float4(dirView, 0)).xyz );
        outOrigin = cam->getPosition();
        outDirection = dirWorld;
    } else {
        float3 worldPoint = (invView * float4(viewPoint, 1)).xyz;
        outOrigin = worldPoint;
        outDirection = normalize(cam->getForwardVector());
    }
    return true;
}

void PickingRegistry::clear() {
    mMeshes.clear();
}

void PickingRegistry::logMemoryUsage() const {
#ifndef NDEBUG
    if (mMeshes.empty()) {
        slog.i << "PickingRegistry: No entities registered" << io::endl;
        return;
    }

    size_t totalOriginalIndicesBytes = 0;
    size_t totalExpandedIndicesBytes = 0;
    size_t entitiesWithExpandedIndices = 0;

    for (const auto& [entity, meshData] : mMeshes) {
        // Calculate memory for original indices
        size_t originalBytes = meshData.indices.size() * sizeof(uint32_t);
        totalOriginalIndicesBytes += originalBytes;

        // Calculate memory for expanded indices (if present)
        if (!meshData.expandedIndices.empty()) {
            size_t expandedBytes = meshData.expandedIndices.size() * sizeof(uint32_t);
            totalExpandedIndicesBytes += expandedBytes;
            entitiesWithExpandedIndices++;
        }
    }

    // Convert to KB and MB for readability
    double totalOriginalKB = totalOriginalIndicesBytes / 1024.0;
    double totalOriginalMB = totalOriginalKB / 1024.0;
    double totalExpandedKB = totalExpandedIndicesBytes / 1024.0;
    double totalExpandedMB = totalExpandedKB / 1024.0;
    double totalBytes = totalOriginalIndicesBytes + totalExpandedIndicesBytes;
    double totalKB = totalBytes / 1024.0;
    double totalMB = totalKB / 1024.0;

    slog.i << "========================================" << io::endl;
    slog.i << "PickingRegistry Memory Usage Summary" << io::endl;
    slog.i << "========================================" << io::endl;
    slog.i << "Total entities: " << mMeshes.size() << io::endl;
    slog.i << "Entities with expanded indices: " << entitiesWithExpandedIndices << io::endl;
    slog.i << "----------------------------------------" << io::endl;
    slog.i << "Original indices: " << totalOriginalIndicesBytes << " bytes "
           << "(" << totalOriginalKB << " KB, " << totalOriginalMB << " MB)" << io::endl;
    slog.i << "Expanded indices: " << totalExpandedIndicesBytes << " bytes "
           << "(" << totalExpandedKB << " KB, " << totalExpandedMB << " MB)" << io::endl;
    slog.i << "----------------------------------------" << io::endl;
    slog.i << "TOTAL: " << totalBytes << " bytes "
           << "(" << totalKB << " KB, " << totalMB << " MB)" << io::endl;
    slog.i << "========================================" << io::endl;
#endif
}

} // namespace filament::gltfio
