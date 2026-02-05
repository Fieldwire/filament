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

// TinyBVH - Fast BVH-based ray tracing
// Source: https://github.com/jbikker/tinybvh (commit 4b5b649)
#define TINYBVH_IMPLEMENTATION
#include "../../third_party/tiny-bvh/tiny_bvh.h"

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
            // No indices - vertices define triangles directly (v0, v1, v2), (v3, v4, v5), ...
            slog.w << "PickingRegistry: Mesh primitive has no index buffer, generating sequential indices (entity "
                   << entity.getId() << ")" << io::endl;

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

    slog.d << "PickingRegistry: Registered mesh for entity " << entity.getId()
           << " with " << (meshData.indices.size() / 3) << " triangles" << io::endl;

    return true;
}

void PickingRegistry::unregisterMesh(utils::Entity entity) {
    auto it = mMeshes.find(entity);
    if (it != mMeshes.end()) {
        mMeshes.erase(it);
        slog.d << "PickingRegistry: Unregistered mesh for entity " << entity.getId() << io::endl;
    }
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

    // Convert triangle data to tinybvh format (vec4 per vertex, 3 vertices per triangle)
    std::vector<tinybvh::bvhvec4> bvhTriangles;
    bvhTriangles.reserve(triangleCount * 3);

    for (size_t i = 0; i < triangleCount; ++i) {
        uint32_t idx0 = meshData.indices[i * 3 + 0];
        uint32_t idx1 = meshData.indices[i * 3 + 1];
        uint32_t idx2 = meshData.indices[i * 3 + 2];

        const float3& v0 = meshData.positions[idx0];
        const float3& v1 = meshData.positions[idx1];
        const float3& v2 = meshData.positions[idx2];

        bvhTriangles.push_back(tinybvh::bvhvec4(v0.x, v0.y, v0.z, 0.0f));
        bvhTriangles.push_back(tinybvh::bvhvec4(v1.x, v1.y, v1.z, 0.0f));
        bvhTriangles.push_back(tinybvh::bvhvec4(v2.x, v2.y, v2.z, 0.0f));
    }

    // Build the BVH - TinyBVH handles all AABB computation and tree construction
    meshData.bvh->Build(bvhTriangles.data(), static_cast<unsigned int>(triangleCount));

    slog.d << "PickingRegistry: Built BVH for " << triangleCount << " triangles" << io::endl;
}

// ============================================================================
// Query Methods
// ============================================================================

const MeshData* PickingRegistry::getMeshData(utils::Entity entity) const {
    auto it = mMeshes.find(entity);
    return it != mMeshes.end() ? &it->second : nullptr;
}

bool PickingRegistry::hasMesh(utils::Entity entity) const {
    return mMeshes.find(entity) != mMeshes.end();
}

size_t PickingRegistry::getMeshCount() const {
    return mMeshes.size();
}

void PickingRegistry::clear() {
    mMeshes.clear();
    slog.d << "PickingRegistry: Cleared all meshes" << io::endl;
}
} // namespace filament::gltfio
