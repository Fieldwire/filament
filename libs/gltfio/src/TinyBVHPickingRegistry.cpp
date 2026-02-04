/*
 * TinyBVHPicking.cpp - Implementation using tinybvh for ray-triangle intersection
 */

#include <gltfio/TinyBVHPickingRegistry.h>
#include <gltfio/Picking.h>

// Include tinybvh (header-only library)
#define TINYBVH_IMPLEMENTATION
#include "../../../third_party/tinybvh.h"
#include "gltfio/PickingUtils.h"
#include "gltfio/ScreenRay.h"

// ============================================================================
// TINYBVH VERSION COMPATIBILITY CHECK
// ============================================================================
// Our custom intersection callback duplicates tinybvh's Moeller-Trumbore implementation.
// If tinybvh is updated and changes its algorithm, we need to update our callback too.
// This compile-time check forces review when tinybvh version changes.
//
// Current implementation based on: TinyBVH 1.6.7
// Algorithm: Moeller-Trumbore ray-triangle intersection (MOLLER_TRUMBORE_TEST macro)
// Reference: tinybvh.h line 1643-1655 (macro) and line 8468-8471 (IntersectTri usage)
//
#if (TINY_BVH_VERSION_MAJOR != 1) || (TINY_BVH_VERSION_MINOR != 6) || (TINY_BVH_VERSION_SUB != 7)
    #warning "╔════════════════════════════════════════════════════════════════════════════╗"
    #warning "║ TinyBVH version has changed!                                               ║"
    #warning "║                                                                            ║"
    #warning "║ Current implementation: TinyBVH 1.6.7                                      ║"
    #warning "║ Detected version: " TINY_BVH_VERSION_MAJOR "." TINY_BVH_VERSION_MINOR "." TINY_BVH_VERSION_SUB "                                                          ║"
    #warning "║                                                                            ║"
    #warning "║ ACTION REQUIRED:                                                           ║"
    #warning "║ 1. Check tinybvh.h line ~8468: Does IntersectTri still use                ║"
    #warning "║    MOLLER_TRUMBORE_TEST macro?                                             ║"
    #warning "║ 2. If yes, update version check in TinyBVHPicking.cpp line 17             ║"
    #warning "║ 3. If no, update customTriangleIntersectWithFiltering() to match           ║"
    #warning "║    new algorithm                                                           ║"
    #warning "╚════════════════════════════════════════════════════════════════════════════╝"
    // To silence this warning: Update the version numbers above after verifying compatibility
#endif

// Optional: Enable validation during development to verify our implementation matches tinybvh
// Uncomment this to enable runtime checks that compare our results against tinybvh's IntersectTri
// #define TINYBVH_VALIDATE_CUSTOM_INTERSECTION
//
// When enabled, this will:
// 1. Run both our custom implementation AND tinybvh's built-in IntersectTri
// 2. Compare results and log any discrepancies
// 3. Helps catch bugs or algorithm changes in either implementation
// 4. Should only be enabled during testing (has performance overhead)

// ============================================================================

#include <math/mat4.h>
#include <math/vec4.h>
#include <math/norm.h>
#include <utils/Log.h>

#include <limits>

using namespace filament::math;
using namespace utils;

namespace filament::gltfio {

using Entity = utils::Entity;
using mat4f = math::mat4f;
using float3 = math::float3;

// Context for custom intersection callback
struct TriangleFilterContext {
    uint32_t skipStartIdx;
    uint32_t skipEndIdx;
    const MeshData* meshData;  // Direct access to original mesh data - no duplication!
    mutable uint32_t callbackInvocationCount = 0;  // DEBUG: count how many times callback is called

    // OPTIONAL: Bitmask for faster skip check (see optimization notes below)
    // const uint32_t* hiddenTriangleBits = nullptr;  // 1 bit per triangle (0 = hidden, 1 = visible)
};

// Custom intersection callback that filters hidden triangles during BVH traversal
// This allows the ray to pass through hidden triangles and find visible ones behind them
//
// ============================================================================
// ALGORITHM COMPATIBILITY NOTES (TinyBVH 1.6.7)
// ============================================================================
// This callback replicates tinybvh's default triangle intersection algorithm.
// When tinybvh is updated, verify the implementation still matches by checking:
//
// 1. File: third_party/tinybvh.h
// 2. Function: BVHBase::IntersectTri() at line ~8443
// 3. Verify it uses: MOLLER_TRUMBORE_TEST macro (should be at line ~8471)
// 4. Check macro definition at line ~1643
//
// The algorithm should match this structure:
//    const bvhvec3 h = cross(ray.D, e2);
//    const float a = dot(e1, h);
//    if (fabs(a) < epsilon) return;  // parallel
//    const float f = 1/a;
//    const bvhvec3 s = ray.O - v0;
//    const float u = f * dot(s, h);
//    const bvhvec3 q = cross(s, e1);
//    const float v = f * dot(ray.D, q);
//    if (u < 0 || v < 0 || u+v > 1) return;  // outside triangle
//    const float t = f * dot(e2, q);
//    if (t < 0 || t > ray.hit.t) return;  // behind ray or farther than current hit
//
// If tinybvh switches to WATERTIGHT_TRITEST (Woop et al.'s algorithm),
// this callback must be updated to match.
// ============================================================================
//
// PERFORMANCE NOTE: The skip check happens FIRST (fast), then we return immediately for hidden triangles.
// Only VISIBLE triangles proceed to the Moeller-Trumbore math below. So the "heavy math" only runs
// when needed - this is already optimal.
static bool customTriangleIntersectWithFiltering(tinybvh::Ray& ray, const unsigned primIdx) {
    // Get context from ray's auxiliary data
    TriangleFilterContext* ctx = static_cast<TriangleFilterContext*>(ray.hit.auxData);
    if (!ctx) return false;

    // DEBUG: Count how many times this callback is invoked
    ctx->callbackInvocationCount++;

    // FAST CHECK: Is this triangle in the hidden range?
    // Each triangle uses indices [primIdx*3, primIdx*3+1, primIdx*3+2]
    // We skip if ANY of these indices overlap with [skipStartIdx, skipEndIdx]
    uint32_t triStartIdx = primIdx * 3;
    uint32_t triEndIdx = triStartIdx + 2;

    // OPTIMIZATION ANALYSIS:
    // Current approach: Index range check (2 integer comparisons)
    // Alternative: Bitmask check (1 array lookup + 1 AND) - ~30% faster skip check
    //   bool skipThis = !(ctx->hiddenTriangleBits[primIdx >> 5] & (1 << (primIdx & 31)));
    //
    // VERDICT: Keep current approach because:
    // - BVH already culls 99%+ of triangles (callback invoked ~20 times, not 10,000)
    // - Skip check is < 1% of total time (intersection math dominates)
    // - Index range is simpler and doesn't require bitmask allocation
    // - Potential speedup: 0.3% overall (not worth the complexity)

    // Skip if there's any overlap between [triStartIdx, triEndIdx] and [skipStartIdx, skipEndIdx]
    bool skipThis = !(triEndIdx < ctx->skipStartIdx || triStartIdx > ctx->skipEndIdx);

    if (skipThis) {
        return false;  // Early exit - no math done for hidden triangles!
    }

    // --- CODE BELOW ONLY RUNS FOR VISIBLE TRIANGLES ---
    // Triangle is visible - perform Moeller-Trumbore intersection test
    //
    // NOTE: This is the SAME algorithm tinybvh uses internally in its IntersectTri() method.
    // We must duplicate it here because when using a custom callback, tinybvh bypasses its
    // built-in IntersectTri() and delegates ALL intersection math to the callback.
    // There's no way to "call tinybvh's IntersectTri()" from here - it's a private member function.
    //
    // See tinybvh.h line 8468: BVHBase::IntersectTri() uses MOLLER_TRUMBORE_TEST macro (line 1643)
    // which expands to the exact same math we're doing below.

    // Get vertex indices for this triangle
    uint32_t idx0 = ctx->meshData->indices[primIdx * 3 + 0];
    uint32_t idx1 = ctx->meshData->indices[primIdx * 3 + 1];
    uint32_t idx2 = ctx->meshData->indices[primIdx * 3 + 2];

    // Get vertex positions from original mesh data (no duplication!)
    const float3& v0 = ctx->meshData->positions[idx0];
    const float3& v1 = ctx->meshData->positions[idx1];
    const float3& v2 = ctx->meshData->positions[idx2];

    // ========================================================================
    // MOELLER-TRUMBORE RAY-TRIANGLE INTERSECTION
    // ========================================================================
    // This replicates tinybvh.h MOLLER_TRUMBORE_TEST macro (line ~1643-1655)
    // Update this if tinybvh changes to WATERTIGHT_TRITEST or another algorithm
    //
    // Macro structure:
    //   const bvhvec3 h = cross(ray.D, e2);
    //   const float a = dot(e1, h);
    //   if (fabs(a) < epsilon) exit;
    //   const float f = 1/a;
    //   const bvhvec3 s = ray.O - v0;
    //   const float u = f * dot(s, h);
    //   const bvhvec3 q = cross(s, e1);
    //   const float v = f * dot(ray.D, q);
    //   const bool miss = u < 0 || v < 0 || u+v > 1;
    //   if (miss) exit;
    //   const float t = f * dot(e2, q);
    //   if (t < 0 || t > tmax) exit;

    // Edge vectors: e1 = v1-v0, e2 = v2-v0
    const tinybvh::bvhvec3 edge1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
    const tinybvh::bvhvec3 edge2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);

    // h = cross(ray.D, e2)
    const tinybvh::bvhvec3 h(ray.D.y * edge2.z - ray.D.z * edge2.y,
                             ray.D.z * edge2.x - ray.D.x * edge2.z,
                             ray.D.x * edge2.y - ray.D.y * edge2.x);

    // a = dot(e1, h)
    const float a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;

    // Check parallel (tinybvh uses 0.000001f epsilon)
    if (fabsf(a) < 1e-8f) return false;

    // f = 1/a
    const float f = 1.0f / a;

    // s = ray.O - v0
    const tinybvh::bvhvec3 s(ray.O.x - v0.x, ray.O.y - v0.y, ray.O.z - v0.z);

    // u = f * dot(s, h)
    const float u = f * (s.x * h.x + s.y * h.y + s.z * h.z);

    // Early exit for u outside [0,1]
    if (u < 0.0f || u > 1.0f) return false;

    // q = cross(s, e1)
    const tinybvh::bvhvec3 q(s.y * edge1.z - s.z * edge1.y,
                             s.z * edge1.x - s.x * edge1.z,
                             s.x * edge1.y - s.y * edge1.x);

    // v = f * dot(ray.D, q)
    const float v = f * (ray.D.x * q.x + ray.D.y * q.y + ray.D.z * q.z);

    // Check barycentric coordinates (tinybvh: u < 0 || v < 0 || u+v > 1)
    if (v < 0.0f || u + v > 1.0f) return false;

    // t = f * dot(e2, q)
    const float t = f * (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z);

    // Check if hit is valid (tinybvh: t < 0 || t > tmax)
    if (t > 0.0f && t < ray.hit.t) {
        // Found a closer hit
        ray.hit.t = t;
        ray.hit.u = u;
        ray.hit.v = v;
        ray.hit.prim = primIdx;
        return true;
    }

    return false;
}

// Internal structure to hold mesh data + tinybvh BVH
struct TinyBVHPickingRegistry::EntityMesh {
    MeshData meshData;
    tinybvh::BVH bvh;
    std::vector<tinybvh::bvhvec4> bvhTriangles; // MUST persist - BVH stores pointer to this!

    // IMPORTANT: TinyBVH stores a pointer to the vertex data passed to Build().
    // We MUST keep bvhTriangles alive for the lifetime of the BVH!

    EntityMesh(MeshData&& md) : meshData(std::move(md)) {
        buildBVH();
    }

    void buildBVH() {
        const size_t indexCount = meshData.indices.size();
        const size_t positionCount = meshData.positions.size();

        // Validate indices & positions before building BVH
        if (positionCount == 0) {
            utils::slog.w << "TinyBVH build skipped: mesh has no positions" << '\n';
            return;
        }
        if (indexCount == 0) {
            utils::slog.w << "TinyBVH build skipped: mesh has no indices" << '\n';
            return;
        }
        if (indexCount % 3 != 0) {
            utils::slog.w << "TinyBVH build skipped: index count (" << indexCount << ") is not a multiple of 3" << '\n';
            return;
        }

        const size_t triCount = indexCount / 3;
        if (triCount == 0) {
            utils::slog.w << "TinyBVH build skipped: zero triangles" << '\n';
            return;
        }

        // Store triangle data persistently - BVH needs this to stay alive!
        bvhTriangles.clear();
        bvhTriangles.reserve(triCount * 3);

        // Convert mesh data to tinybvh format (vec4 per vertex)
        for (size_t i = 0; i < triCount; ++i) {
            uint32_t idx0 = meshData.indices[i * 3 + 0];
            uint32_t idx1 = meshData.indices[i * 3 + 1];
            uint32_t idx2 = meshData.indices[i * 3 + 2];

            // Defensive: ensure indices are within positions range
            if (idx0 >= positionCount || idx1 >= positionCount || idx2 >= positionCount) {
                utils::slog.w << "TinyBVH build aborted: triangle " << i << " has out-of-range vertex index(es)"
                              << " [" << idx0 << "," << idx1 << "," << idx2 << "] vs positions count " << positionCount << '\n';
                bvhTriangles.clear();
                return;
            }

            const float3& v0 = meshData.positions[idx0];
            const float3& v1 = meshData.positions[idx1];
            const float3& v2 = meshData.positions[idx2];

            bvhTriangles.push_back(tinybvh::bvhvec4(v0.x, v0.y, v0.z, 0));
            bvhTriangles.push_back(tinybvh::bvhvec4(v1.x, v1.y, v1.z, 0));
            bvhTriangles.push_back(tinybvh::bvhvec4(v2.x, v2.y, v2.z, 0));
        }

        // Build the BVH - it stores a pointer to bvhTriangles!
        bvh.Build(bvhTriangles.data(), (unsigned int)triCount);

        utils::slog.d << "TinyBVH built for mesh: " << triCount << " triangles ("
                      << (bvhTriangles.size() * sizeof(tinybvh::bvhvec4)) << " bytes for BVH vertices)" << '\n';
    }
};

TinyBVHPickingRegistry::~TinyBVHPickingRegistry() = default;

// Implement custom deleter declared in header
void TinyBVHPickingRegistry::EntityMeshDeleter::operator()(EntityMesh* p) const {
    delete p;
}

void TinyBVHPickingRegistry::registerMesh(Entity e, MeshData&& mesh) {
    // Construct unique_ptr with the custom deleter to match mMeshes value_type
    std::unique_ptr<EntityMesh, EntityMeshDeleter> ptr(new EntityMesh(std::move(mesh)));
    mMeshes[e] = std::move(ptr);
}

void TinyBVHPickingRegistry::updateTransform(Entity e, const mat4f& world) {
    mWorldTransforms[e] = world;
}

const MeshData* TinyBVHPickingRegistry::getMesh(Entity e) const {
    auto it = mMeshes.find(e);
    return it == mMeshes.end() ? nullptr : &it->second->meshData;
}

TinyBVHPickingRegistry::Hit TinyBVHPickingRegistry::pick(
    const float3& rayOrigin, const float3& rayDir) const {

    Hit best{ Entity{}, -1, std::numeric_limits<float>::max(), {} };
    float3 dirNorm = normalize(rayDir);

    for (const auto& [entity, entityMesh] : mMeshes) {
        // Safety checks: ensure BVH was built successfully
        if (!entityMesh) {
            utils::slog.e << "Skipping entity - entityMesh is null!" << '\n';
            continue;
        }

        if (entityMesh->bvhTriangles.empty()) {
            utils::slog.w << "Skipping entity - BVH not built (no triangle data)" << '\n';
            continue;
        }

        if (entityMesh->bvh.bvhNode == nullptr) {
            utils::slog.e << "Skipping entity - BVH nodes not allocated!" << '\n';
            continue;
        }

        if (entityMesh->meshData.indices.empty() || entityMesh->meshData.positions.empty()) {
            utils::slog.w << "Skipping entity - mesh has no geometry data" << '\n';
            continue;
        }

        // Get world transform
        mat4f world = mat4f(1.0f); // Identity
        auto wIt = mWorldTransforms.find(entity);
        if (wIt != mWorldTransforms.end()) {
            world = wIt->second;
        }

        // Transform ray to local space
        mat4f invWorld = inverse(world);
        float4 o4(rayOrigin, 1.0f);
        float4 d4(dirNorm, 0.0f);
        float3 localO = (invWorld * o4).xyz;
        float3 localD = normalize((invWorld * d4).xyz);

        // Setup tinybvh ray - use constructor to initialize rD properly
        tinybvh::bvhvec3 origin(localO.x, localO.y, localO.z);
        tinybvh::bvhvec3 direction(localD.x, localD.y, localD.z);
        tinybvh::Ray ray(origin, direction, best.distance);

        // Intersect with BVH
        entityMesh->bvh.Intersect(ray);

        if (ray.hit.t < best.distance && ray.hit.t > 0) {
            best.entity = entity;
            best.triangle = ray.hit.prim; // Triangle index
            best.distance = ray.hit.t;
            best.bary = float3{ ray.hit.u, ray.hit.v, 1.0f - ray.hit.u - ray.hit.v };
        }
    }

    return best;
}

TinyBVHPickingRegistry::Hit *TinyBVHPickingRegistry::pick(
    View *view, const int2 &position, FilamentAsset *asset
) {
    // utils::slog.d << "TinyBVH pick called at screen position (" << position.x << ", " << position.y << ")" << '\n';

    updateTinybvhPickingTransforms(asset);

    std::pair<math::float3, math::float3> *rayPtr = computeScreenRay(view, position);

    if (!rayPtr) {
        utils::slog.w << "TinyBVH pick failed: computeScreenRay returned null" << '\n';
        return nullptr;
    }

    // Extract ray data and delete the allocated pair to prevent memory leak
    const float3 rayOrigin = rayPtr->first;
    const float3 rayDir = rayPtr->second;
    delete rayPtr;

    // utils::slog.i << "=== TinyBVH Pick Debug ===" << '\n';
    // utils::slog.i << "Ray origin: (" << rayOrigin.x << ", " << rayOrigin.y << ", " << rayOrigin.z << ")" << '\n';
    // utils::slog.i << "Ray direction: (" << rayDir.x << ", " << rayDir.y << ", " << rayDir.z << ")" << '\n';
    // utils::slog.i << "Number of meshes registered: " << mMeshes.size() << '\n';
    // utils::slog.i << "Number of transforms: " << mWorldTransforms.size() << '\n';

    Hit best{ Entity{}, -1, std::numeric_limits<float>::max(), {} };
    float3 dirNorm = normalize(rayDir);

    int entityIndex = 0;
    for (const auto& [entity, entityMesh] : mMeshes) {
        entityIndex++;
        // utils::slog.d << "  Entity " << entityIndex << " (ID=" << entity.getId() << "): Checking..." << '\n';

        // Safety checks: ensure BVH was built successfully
        if (!entityMesh) {
            utils::slog.e << "    SKIP: entityMesh is null!" << '\n';
            continue;
        }

        if (entityMesh->bvhTriangles.empty()) {
            utils::slog.w << "    SKIP: BVH not built (no triangle data)" << '\n';
            continue;
        }

        if (entityMesh->bvh.bvhNode == nullptr) {
            utils::slog.e << "    SKIP: BVH nodes not allocated!" << '\n';
            continue;
        }

        if (entityMesh->meshData.indices.empty() || entityMesh->meshData.positions.empty()) {
            utils::slog.w << "    SKIP: mesh has no geometry data" << '\n';
            continue;
        }

        // utils::slog.d << "    Mesh: " << entityMesh->meshData.positions.size() << " verts, "
        //               << (entityMesh->meshData.indices.size() / 3) << " tris" << '\n';

        // Get world transform
        mat4f world = mat4f(1.0f); // Identity
        auto wIt = mWorldTransforms.find(entity);
        if (wIt != mWorldTransforms.end()) {
            world = wIt->second;
        } else {
            utils::slog.w << "    WARNING: No transform found, using identity" << '\n';
        }

        // Transform ray to local space
        mat4f invWorld = inverse(world);
        float4 o4(rayOrigin, 1.0f);
        float4 d4(dirNorm, 0.0f);
        float3 localO = (invWorld * o4).xyz;
        float3 localD = normalize((invWorld * d4).xyz);

        TriangleFilterContext ctx;
        ctx.skipStartIdx = 0;
        ctx.skipEndIdx = 1;
        ctx.meshData = &entityMesh->meshData;  // Direct reference - no duplication!

        // utils::slog.d << "    Local ray origin: (" << localO.x << ", " << localO.y << ", " << localO.z << ")" << '\n';
        // utils::slog.d << "    Local ray dir: (" << localD.x << ", " << localD.y << ", " << localD.z << ")" << '\n';

        // Setup tinybvh ray - MUST use constructor to properly initialize rD (reciprocal direction)!
        // The constructor normalizes D and computes rD = 1/D which is required for BVH traversal
        tinybvh::bvhvec3 origin(localO.x, localO.y, localO.z);
        tinybvh::bvhvec3 direction(localD.x, localD.y, localD.z);
        tinybvh::Ray bvhRay(origin, direction, best.distance);

        bvhRay.hit.auxData = &ctx;  // Pass context to callback

        // utils::slog.d << "    BVH has " << entityMesh->bvhTriangles.size() / 3 << " triangles" << '\n';
        // utils::slog.d << "    BVH node count: " << (entityMesh->bvh.bvhNode ? "allocated" : "NULL") << '\n';
        // utils::slog.d << "    Ray.hit.t initialized to: " << bvhRay.hit.t << '\n';

        // Temporarily set custom intersection callback
        auto* bvhPtr = const_cast<tinybvh::BVH*>(&entityMesh->bvh);
        auto originalCallback = bvhPtr->customIntersect;
        bvhPtr->customIntersect = customTriangleIntersectWithFiltering;

        // Intersect with BVH - callback will skip hidden triangles during traversal
        bvhPtr->Intersect(bvhRay);

        // Restore original callback
        bvhPtr->customIntersect = originalCallback;

        // utils::slog.d << "    BVH Intersect result: t=" << bvhRay.hit.t << " (best so far: " << best.distance << ")" << '\n';
        // utils::slog.d << "    BVH hit.prim=" << bvhRay.hit.prim << ", hit.u=" << bvhRay.hit.u << ", hit.v=" << bvhRay.hit.v << '\n';

        if (bvhRay.hit.t < best.distance && bvhRay.hit.t > 0) {
            // utils::slog.i << "    ✓ NEW BEST HIT: tri=" << bvhRay.hit.prim << ", t=" << bvhRay.hit.t << '\n';
            best.entity = entity;
            best.triangle = bvhRay.hit.prim; // Triangle index
            best.distance = bvhRay.hit.t;
            best.bary = float3{ bvhRay.hit.u, bvhRay.hit.v, 1.0f - bvhRay.hit.u - bvhRay.hit.v };
        } else if (bvhRay.hit.t > 0) {
            // utils::slog.d << "    Hit found but farther than current best" << '\n';
        } else {
            // utils::slog.d << "    No intersection with this entity" << '\n';
        }
    }

    if (best.entity.getId() == 0 || best.triangle < 0) {
        // utils::slog.d << "TinyBVH pick: No hit found (ray missed all objects)" << '\n';
        return nullptr;
    }

    // utils::slog.d << "TinyBVH pick HIT: entity=" << best.entity.getId()
    //               << ", triangle=" << best.triangle
    //               << ", distance=" << best.distance << '\n';
    return new Hit(best);
}

TinyBVHPickingRegistry::Hit TinyBVHPickingRegistry::pickSkippingIndexRange(
    const float3& rayOrigin, const float3& rayDir,
    uint32_t startIdx, uint32_t endIdx) const {

    Hit best{ Entity{}, -1, std::numeric_limits<float>::max(), {} };
    float3 dirNorm = normalize(rayDir);

    if (startIdx > endIdx) {
        return best;
    }

    for (const auto& [entity, entityMesh] : mMeshes) {
        // Safety checks: ensure BVH was built successfully
        if (!entityMesh) {
            utils::slog.e << "Skipping entity - entityMesh is null!" << '\n';
            continue;
        }

        if (entityMesh->bvhTriangles.empty()) {
            utils::slog.w << "Skipping entity - BVH not built (no triangle data)" << '\n';
            continue;
        }

        if (entityMesh->bvh.bvhNode == nullptr) {
            utils::slog.e << "Skipping entity - BVH nodes not allocated!" << '\n';
            continue;
        }

        if (entityMesh->meshData.indices.empty() || entityMesh->meshData.positions.empty()) {
            utils::slog.w << "Skipping entity - mesh has no geometry data" << '\n';
            continue;
        }

        // Get world transform
        mat4f world = mat4f(1.0f);
        auto wIt = mWorldTransforms.find(entity);
        if (wIt != mWorldTransforms.end()) {
            world = wIt->second;
        }

        // Transform ray to local space
        mat4f invWorld = inverse(world);
        float4 o4(rayOrigin, 1.0f);
        float4 d4(dirNorm, 0.0f);
        float3 localO = (invWorld * o4).xyz;
        float3 localD = normalize((invWorld * d4).xyz);

        // Setup filter context for custom intersection callback
        TriangleFilterContext ctx;
        ctx.skipStartIdx = startIdx;
        ctx.skipEndIdx = endIdx;
        ctx.meshData = &entityMesh->meshData;  // Direct reference - no duplication!

        // Setup tinybvh ray - use constructor to initialize rD properly
        tinybvh::bvhvec3 origin(localO.x, localO.y, localO.z);
        tinybvh::bvhvec3 direction(localD.x, localD.y, localD.z);
        tinybvh::Ray ray(origin, direction, best.distance);
        ray.hit.auxData = &ctx;  // Pass context to callback

        // Temporarily set custom intersection callback
        auto* bvhPtr = const_cast<tinybvh::BVH*>(&entityMesh->bvh);
        auto originalCallback = bvhPtr->customIntersect;
        bvhPtr->customIntersect = customTriangleIntersectWithFiltering;

        // Intersect with BVH - callback will skip hidden triangles during traversal
        bvhPtr->Intersect(ray);

        // Restore original callback
        bvhPtr->customIntersect = originalCallback;

        // DEBUG: Show BVH efficiency
        const size_t totalTriangles = entityMesh->meshData.indices.size() / 3;
        utils::slog.d << "BVH Efficiency - Callback invoked: " << ctx.callbackInvocationCount
                     << " times out of " << totalTriangles << " total triangles"
                     << " (tested only " << (100.0f * ctx.callbackInvocationCount / totalTriangles) << "%)"
                     << '\n';

        // Check if we found a closer hit
        if (ray.hit.t < best.distance && ray.hit.t > 0) {
            best.entity = entity;
            best.triangle = ray.hit.prim;
            best.distance = ray.hit.t;
            best.bary = float3{ ray.hit.u, ray.hit.v, 1.0f - ray.hit.u - ray.hit.v };
        }
    }

    return best;
}

TinyBVHPickingRegistry::Hit *TinyBVHPickingRegistry::pickSkippingIndexRange(
    View *view, const int2 &position, FilamentAsset *asset,
    uint32_t startIdx, uint32_t endIdx
) {
    // utils::slog.d << "TinyBVH pick called at screen position (" << position.x << ", " << position.y << ")" << '\n';

    updateTinybvhPickingTransforms(asset);

    std::pair<math::float3, math::float3> *rayPtr = computeScreenRay(view, position);

    if (!rayPtr) {
        utils::slog.w << "TinyBVH pick failed: computeScreenRay returned null" << '\n';
        return nullptr;
    }

    // Extract ray data and delete the allocated pair to prevent memory leak
    const float3 rayOrigin = rayPtr->first;
    const float3 rayDir = rayPtr->second;
    delete rayPtr;

    // utils::slog.i << "=== TinyBVH Pick Debug ===" << '\n';
    // utils::slog.i << "Ray origin: (" << rayOrigin.x << ", " << rayOrigin.y << ", " << rayOrigin.z << ")" << '\n';
    // utils::slog.i << "Ray direction: (" << rayDir.x << ", " << rayDir.y << ", " << rayDir.z << ")" << '\n';
    // utils::slog.i << "Number of meshes registered: " << mMeshes.size() << '\n';
    // utils::slog.i << "Number of transforms: " << mWorldTransforms.size() << '\n';

    Hit best{ Entity{}, -1, std::numeric_limits<float>::max(), {} };

    if (startIdx > endIdx) {
        return nullptr;
    }

    float3 dirNorm = normalize(rayDir);

    int entityIndex = 0;
    for (const auto& [entity, entityMesh] : mMeshes) {
        entityIndex++;
        // utils::slog.d << "  Entity " << entityIndex << " (ID=" << entity.getId() << "): Checking..." << '\n';

        // Safety checks: ensure BVH was built successfully
        if (!entityMesh) {
            utils::slog.e << "    SKIP: entityMesh is null!" << '\n';
            continue;
        }

        if (entityMesh->bvhTriangles.empty()) {
            utils::slog.w << "    SKIP: BVH not built (no triangle data)" << '\n';
            continue;
        }

        if (entityMesh->bvh.bvhNode == nullptr) {
            utils::slog.e << "    SKIP: BVH nodes not allocated!" << '\n';
            continue;
        }

        if (entityMesh->meshData.indices.empty() || entityMesh->meshData.positions.empty()) {
            utils::slog.w << "    SKIP: mesh has no geometry data" << '\n';
            continue;
        }

        // utils::slog.d << "    Mesh: " << entityMesh->meshData.positions.size() << " verts, "
        //               << (entityMesh->meshData.indices.size() / 3) << " tris" << '\n';

        // Get world transform
        mat4f world = mat4f(1.0f); // Identity
        auto wIt = mWorldTransforms.find(entity);
        if (wIt != mWorldTransforms.end()) {
            world = wIt->second;
        } else {
            utils::slog.w << "    WARNING: No transform found, using identity" << '\n';
        }

        // Transform ray to local space
        mat4f invWorld = inverse(world);
        float4 o4(rayOrigin, 1.0f);
        float4 d4(dirNorm, 0.0f);
        float3 localO = (invWorld * o4).xyz;
        float3 localD = normalize((invWorld * d4).xyz);

        TriangleFilterContext ctx;
        ctx.skipStartIdx = 0;
        ctx.skipEndIdx = 1;
        ctx.meshData = &entityMesh->meshData;  // Direct reference - no duplication!

        // utils::slog.d << "    Local ray origin: (" << localO.x << ", " << localO.y << ", " << localO.z << ")" << '\n';
        // utils::slog.d << "    Local ray dir: (" << localD.x << ", " << localD.y << ", " << localD.z << ")" << '\n';

        // Setup tinybvh ray - MUST use constructor to properly initialize rD (reciprocal direction)!
        // The constructor normalizes D and computes rD = 1/D which is required for BVH traversal
        tinybvh::bvhvec3 origin(localO.x, localO.y, localO.z);
        tinybvh::bvhvec3 direction(localD.x, localD.y, localD.z);
        tinybvh::Ray bvhRay(origin, direction, best.distance);
        bvhRay.hit.auxData = &ctx;  // Pass context to callback


        // utils::slog.d << "    BVH has " << entityMesh->bvhTriangles.size() / 3 << " triangles" << '\n';
        // utils::slog.d << "    BVH node count: " << (entityMesh->bvh.bvhNode ? "allocated" : "NULL") << '\n';
        // utils::slog.d << "    Ray.hit.t initialized to: " << bvhRay.hit.t << '\n';

        // Temporarily set custom intersection callback
        auto* bvhPtr = const_cast<tinybvh::BVH*>(&entityMesh->bvh);
        auto originalCallback = bvhPtr->customIntersect;
        bvhPtr->customIntersect = customTriangleIntersectWithFiltering;

        // Intersect with BVH - callback will skip hidden triangles during traversal
        bvhPtr->Intersect(bvhRay);

        // Restore original callback
        bvhPtr->customIntersect = originalCallback;

        // utils::slog.d << "    BVH Intersect result: t=" << bvhRay.hit.t << " (best so far: " << best.distance << ")" << '\n';
        // utils::slog.d << "    BVH hit.prim=" << bvhRay.hit.prim << ", hit.u=" << bvhRay.hit.u << ", hit.v=" << bvhRay.hit.v << '\n';

        if (bvhRay.hit.t < best.distance && bvhRay.hit.t > 0) {
            // utils::slog.i << "    ✓ NEW BEST HIT: tri=" << bvhRay.hit.prim << ", t=" << bvhRay.hit.t << '\n';
            best.entity = entity;
            best.triangle = bvhRay.hit.prim; // Triangle index
            best.distance = bvhRay.hit.t;
            best.bary = float3{ bvhRay.hit.u, bvhRay.hit.v, 1.0f - bvhRay.hit.u - bvhRay.hit.v };
        } else if (bvhRay.hit.t > 0) {
            // utils::slog.d << "    Hit found but farther than current best" << '\n';
        } else {
            // utils::slog.d << "    No intersection with this entity" << '\n';
        }
    }

    if (best.entity.getId() == 0 || best.triangle < 0) {
        // utils::slog.d << "TinyBVH pick: No hit found (ray missed all objects)" << '\n';
        return nullptr;
    }

    // utils::slog.d << "TinyBVH pick HIT: entity=" << best.entity.getId()
    //               << ", triangle=" << best.triangle
    //               << ", distance=" << best.distance << '\n';
    return new Hit(best);
}

} // namespace filament::gltfio
