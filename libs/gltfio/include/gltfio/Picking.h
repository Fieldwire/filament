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

#include <vector>

#include <cgltf.h>

#include "filament/View.h"

// Forward declaration to avoid include-order issues.
namespace filament::gltfio {
    class FilamentAsset;
}

namespace filament::gltfio {
using namespace math;    


    // Use the same alias pattern as other gltfio headers.
using Entity = utils::Entity;
using mat4f = mat4f;
using float3 = float3;
using float4 = float4;
using float2 = float2;
using int2 = int2;

struct MeshTriangle { uint32_t i0, i1, i2; };

struct MeshBVHNode {
    float3 min;
    float3 max;
    uint32_t left;   // child index OR start of triangle range if leaf
    uint32_t right;  // child index OR triangle count if leaf
    bool leaf;
};

struct MeshData {
    std::vector<float3> positions;   // local-space vertex positions
    std::vector<float3> normals;     // vertex normals
    std::vector<float4> tangents;    // vertex tangents (w component is handedness)
    std::vector<float2> uvs;         // texture coordinates (TEXCOORD_0)
    std::vector<float4> colors;      // vertex colors (COLOR_0)
    std::vector<uint32_t> indices;                   // triangle list (3 * triCount)
    std::vector<MeshBVHNode> bvh;                    // BVH nodes (empty until built)
    std::vector<uint32_t> leafTris;                  // triangle ordinals for leaves
    Aabb localBounds;                      // un-transformed bounds
    bool bvhBuilt = false;                           // BVH build flag

    // Flags to indicate which attributes are present
    bool hasNormals = false;
    bool hasTangents = false;
    bool hasUVs = false;
    bool hasColors = false;
};

// Build CPU mesh data for picking from a cgltf mesh (triangles only). localBounds left empty.
MeshData buildMeshDataForPicking(const cgltf_mesh* mesh);
} // namespace filament::gltfio

#endif // GLTFIO_PICKING_H
