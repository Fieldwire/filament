/*
 * Picking.cpp - implementation of CPU-side BVH picking runtime (excluding mesh data construction).
 */

#include <cgltf.h> // ensure cgltf definitions available for any forward references (no heavy use here)
#include <gltfio/Picking.h>

#include <math/vec4.h>
#include <math/mat4.h>
#include <math/vec3.h>
#include <math/fast.h>
#include <math/norm.h>

#include <utils/Log.h>

#include <cmath>
#include <algorithm>
#include <limits>
#include <stack>

using namespace filament::math;

namespace filament::gltfio {

// -------------------------------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------------------------------

static inline bool intersectAabb(const float3& rayOrigin, const float3& rayDirInv,
                                 const float3& bmin, const float3& bmax, float tMax) noexcept {
    for (int axis = 0; axis < 3; ++axis) {
        float o = axis == 0 ? rayOrigin.x : (axis == 1 ? rayOrigin.y : rayOrigin.z);
        float dInv = axis == 0 ? rayDirInv.x : (axis == 1 ? rayDirInv.y : rayDirInv.z);
        float minv = axis == 0 ? bmin.x : (axis == 1 ? bmin.y : bmin.z);
        float maxv = axis == 0 ? bmax.x : (axis == 1 ? bmax.y : bmax.z);
        float t0 = (minv - o) * dInv;
        float t1 = (maxv - o) * dInv;
        if (t0 > t1) std::swap(t0, t1);
        if (t1 < 0 || t0 > tMax) return false;
        tMax = std::min(tMax, t1);
    }
    return true;
}

struct BuildTriInfo { uint32_t triIndex; float3 centroid; };

static void buildBvhRecursive(MeshData& mesh, std::vector<BuildTriInfo>& tris,
                              uint32_t start, uint32_t end, uint32_t leafSize,
                              uint32_t& outNodeIndex) {
    float3 bmin{ std::numeric_limits<float>::max() };
    float3 bmax{ -std::numeric_limits<float>::max() };
    for (uint32_t i = start; i < end; ++i) {
        const BuildTriInfo& ti = tris[i];
        uint32_t base = ti.triIndex * 3;
        const float3& v0 = mesh.positions[ mesh.indices[base + 0] ];
        const float3& v1 = mesh.positions[ mesh.indices[base + 1] ];
        const float3& v2 = mesh.positions[ mesh.indices[base + 2] ];
        bmin = min(bmin, v0); bmin = min(bmin, v1); bmin = min(bmin, v2);
        bmax = max(bmax, v0); bmax = max(bmax, v1); bmax = max(bmax, v2);
    }
    MeshBVHNode node{};
    node.min = bmin;
    node.max = bmax;
    uint32_t nodeIndex = (uint32_t)mesh.bvh.size();
    mesh.bvh.push_back(node);

    uint32_t triCount = end - start;
    if (triCount <= leafSize) {
        node.leaf = true;
        node.left = (uint32_t)mesh.leafTris.size();
        node.right = triCount;
        for (uint32_t i = start; i < end; ++i) {
            mesh.leafTris.push_back(tris[i].triIndex);
        }
        mesh.bvh[nodeIndex] = node;
        outNodeIndex = nodeIndex;
        return;
    }

    float3 extent = bmax - bmin;
    int axis = (extent.x > extent.y && extent.x > extent.z) ? 0 : (extent.y > extent.z ? 1 : 2);
    uint32_t mid = start + triCount / 2;
    std::nth_element(tris.begin() + start, tris.begin() + mid, tris.begin() + end,
                     [axis](const BuildTriInfo& a, const BuildTriInfo& b){
                         return (axis==0?a.centroid.x:(axis==1?a.centroid.y:a.centroid.z)) <
                                (axis==0?b.centroid.x:(axis==1?b.centroid.y:b.centroid.z));
                     });

    uint32_t leftChild, rightChild;
    buildBvhRecursive(mesh, tris, start, mid, leafSize, leftChild);
    buildBvhRecursive(mesh, tris, mid, end, leafSize, rightChild);

    node.left = leftChild;
    node.right = rightChild;
    node.leaf = false;
    mesh.bvh[nodeIndex] = node;
    outNodeIndex = nodeIndex;
}

static void buildBVH(MeshData& mesh, uint32_t leafSize = 12) {
    mesh.bvh.clear();
    mesh.leafTris.clear();
    const uint32_t triTotal = (uint32_t)mesh.indices.size() / 3;
    std::vector<BuildTriInfo> triInfos;
    triInfos.reserve(triTotal);
    for (uint32_t t = 0; t < triTotal; ++t) {
        uint32_t base = t * 3;
        const float3& v0 = mesh.positions[ mesh.indices[base + 0] ];
        const float3& v1 = mesh.positions[ mesh.indices[base + 1] ];
        const float3& v2 = mesh.positions[ mesh.indices[base + 2] ];
        float3 c = (v0 + v1 + v2) * float3(1.0f/3.0f);
        triInfos.push_back({ t, c });
    }
    uint32_t rootIndex = 0;
    if (triTotal) {
        buildBvhRecursive(mesh, triInfos, 0, triTotal, leafSize, rootIndex);
    }
    mesh.bvhBuilt = true;
}

// -------------------------------------------------------------------------------------------------
// PickingRegistry methods
// -------------------------------------------------------------------------------------------------

void PickingRegistry::registerMesh(Entity e, MeshData&& mesh) {
    if (mesh.localBounds.isEmpty()) {
        float3 bmin{ std::numeric_limits<float>::max() };
        float3 bmax{ -std::numeric_limits<float>::max() };
        for (auto const& p : mesh.positions) {
            bmin = min(bmin, p);
            bmax = max(bmax, p);
        }
        mesh.localBounds.min = bmin;
        mesh.localBounds.max = bmax;
    }
    if (!mesh.bvhBuilt) {
        buildBVH(mesh);
    }
    mMeshes.emplace(e, std::move(mesh));
}

const MeshData* PickingRegistry::getMesh(Entity e) const {
    auto it = mMeshes.find(e);
    return it == mMeshes.end() ? nullptr : &it->second;
}

MeshData* PickingRegistry::getMeshMutable(Entity e) {
    auto it = mMeshes.find(e);
    return it == mMeshes.end() ? nullptr : &it->second;
}

void PickingRegistry::buildBVHIfNeeded(Entity e) {
    MeshData* md = getMeshMutable(e);
    if (!md) return;
    if (!md->bvhBuilt) {
        buildBVH(*md);
    }
}

void PickingRegistry::updateTransform(Entity e, const mat4f& world) {
    mWorldTransforms[e] = world;
}

static inline bool rayTriangle(const float3& o, const float3& d,
                               const float3& v0, const float3& v1, const float3& v2,
                               float& tOut, float& uOut, float& vOut) noexcept {
    const float3 e1 = v1 - v0;
    const float3 e2 = v2 - v0;
    const float3 p = cross(d, e2);
    float det = dot(e1, p);
    if (fabsf(det) < 1e-8f) return false;
    float invDet = 1.0f / det;
    const float3 tv = o - v0;
    float u = dot(tv, p) * invDet;
    if (u < 0 || u > 1) return false;
    const float3 q = cross(tv, e1);
    float v = dot(d, q) * invDet;
    if (v < 0 || u + v > 1) return false;
    float t = dot(e2, q) * invDet;
    if (t <= 0) return false;
    tOut = t; uOut = u; vOut = v;
    return true;
}

PickingRegistry::Hit PickingRegistry::pick(const float3& rayOrigin, const float3& rayDir) const {
    Hit best{ Entity{}, -1, std::numeric_limits<float>::max(), {} };
    float3 dirNorm = normalize(rayDir);

    for (auto it = mMeshes.begin(); it != mMeshes.end(); ++it) {
        Entity entity = it->first;
        MeshData const& mesh = it->second;
        mat4f world; // identity init (default constructor may not zero)
        auto wIt = mWorldTransforms.find(entity);
        if (wIt != mWorldTransforms.end()) {
            world = wIt->second;
        }
        mat4f invWorld = inverse(world);
        float4 o4(rayOrigin, 1.0f);
        float4 d4(dirNorm, 0.0f);
        float3 localO = (invWorld * o4).xyz;
        float3 localD = normalize((invWorld * d4).xyz);
        float3 localDInv{ 1.0f / localD.x, 1.0f / localD.y, 1.0f / localD.z };

        if (!intersectAabb(localO, localDInv, mesh.localBounds.min, mesh.localBounds.max, best.distance)) {
            continue;
        }
        MeshData const* mdConst = &mesh;
        if (!mdConst->bvhBuilt) {
            const_cast<PickingRegistry*>(this)->buildBVHIfNeeded(entity);
            mdConst = getMesh(entity);
            if (!mdConst) continue;
        }
        MeshData const& md = *mdConst;
        if (md.bvh.empty()) {
            continue;
        }
        struct StackNode { uint32_t index; };
        StackNode stack[128];
        int sp = 0;
        stack[sp++] = { 0u };
        while (sp) {
            uint32_t ni = stack[--sp].index;
            const MeshBVHNode& node = md.bvh[ni];
            if (!intersectAabb(localO, localDInv, node.min, node.max, best.distance)) {
                continue;
            }
            if (node.leaf) {
                uint32_t start = node.left;
                uint32_t count = node.right;
                for (uint32_t i = 0; i < count; ++i) {
                    uint32_t triOrd = md.leafTris[start + i];
                    uint32_t base = triOrd * 3;
                    const float3& v0 = md.positions[ md.indices[base + 0] ];
                    const float3& v1 = md.positions[ md.indices[base + 1] ];
                    const float3& v2 = md.positions[ md.indices[base + 2] ];
                    float t, u, v;
                    if (rayTriangle(localO, localD, v0, v1, v2, t, u, v) && t < best.distance) {
                        best.entity = entity;
                        best.triangle = (int)triOrd;
                        best.distance = t;
                        best.bary = float3{ u, v, 1.0f - u - v };
                    }
                }
            } else {
                stack[sp++] = { node.left };
                stack[sp++] = { node.right };
            }
        }
    }
    return best;
}

} // namespace filament::gltfio
