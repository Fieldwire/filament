/* TinyBVH adapter: builds a temporary BVH per mesh and intersects a world-space ray.
 * This is a minimal bridge for CPU picking.
 */

#define TINYBVH_IMPLEMENTATION
#include <tiny_bvh.h>

#include <gltfio/Picking.h>
#include <math/mat4.h>
#include <math/vec3.h>

#include <vector>

using namespace filament::math;

namespace filament::gltfio {

/* Transforms a point by a 4x4 matrix.
 * Parameters:
 *  m - the transformation matrix
 *  p - the point to transform
 * Returns:
 *  The transformed point
 */
static float3 transformPoint(const mat4f& transformationMatrix, const float3& point) {
    const float4 homogeneousPoint = transformationMatrix * float4(point, 1.0f);
    return homogeneousPoint.xyz;
}

bool trianglesExist(const uint32_t numTriangles, const tinybvh::Ray &ray, const int32_t hitDistance) {
    return hitDistance >= 0 && ray.hit.prim < numTriangles;
}

/* TinyBVH adapter: builds a temporary BVH per mesh and intersects a world-space ray.
 * This is a minimal bridge for CPU picking.
 * Parameters:
 *  meshData   - the mesh data containing positions and indices
 *  world      - the world transform matrix for the mesh
 *  rayOriginWS - the ray origin in world space
 *  rayDirWS    - the ray direction in world space
 * Returns:
 *  PickingRegistry::Hit structure with the hit information
 */
PickingRegistry::Hit tinybvh_pick_mesh_world(
    const MeshData& meshData,
    const mat4f& worldTransformMatrix,
    const float3& rayOriginWorldSpace,
    const float3& rayDirWorldSpace
) {
    PickingRegistry::Hit result{ {}, -1, BVH_FAR };

    const uint32_t numTriangles = static_cast<uint32_t>(meshData.indices.size()) / 3;

    if (numTriangles == 0) return result;

    // Prepare triangle data for tinybvh: array of 3 bvhvec4 per triangle.
    std::vector<tinybvh::bvhvec4> triangles;
    triangles.resize(static_cast<size_t>(numTriangles) * 3);

    for (uint32_t t = 0; t < numTriangles; ++t) {
        uint32_t base = t * 3;
        const float3& v0l = meshData.positions[ meshData.indices[base + 0] ];
        const float3& v1l = meshData.positions[ meshData.indices[base + 1] ];
        const float3& v2l = meshData.positions[ meshData.indices[base + 2] ];

        float3 v0 = transformPoint(worldTransformMatrix, v0l);
        float3 v1 = transformPoint(worldTransformMatrix, v1l);
        float3 v2 = transformPoint(worldTransformMatrix, v2l);

        triangles[base + 0] = tinybvh::bvhvec4(v0.x, v0.y, v0.z, 0.0f);
        triangles[base + 1] = tinybvh::bvhvec4(v1.x, v1.y, v1.z, 0.0f);
        triangles[base + 2] = tinybvh::bvhvec4(v2.x, v2.y, v2.z, 0.0f);
    }

    tinybvh::BVH bvh;
    bvh.Build(triangles.data(), numTriangles);

    tinybvh::Ray ray(
        tinybvh::bvhvec3(rayOriginWorldSpace.x, rayOriginWorldSpace.y, rayOriginWorldSpace.z),
        tinybvh::bvhvec3(rayDirWorldSpace.x, rayDirWorldSpace.y, rayDirWorldSpace.z),
        BVH_FAR
    );

    if (int32_t hitDistance = bvh.Intersect(ray); !trianglesExist(numTriangles, ray, hitDistance)) {
        return result;
    }

    result.triangle = static_cast<int>(ray.hit.prim);
    result.distance = ray.hit.t;

    return result;
}

} // namespace filament::gltfio
