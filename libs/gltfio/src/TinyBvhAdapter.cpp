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

static inline float3 transformPoint(const mat4f& m, const float3& p) {
    float4 r = m * float4(p, 1.0f);
    return r.xyz;
}

PickingRegistry::Hit tinybvh_pick_mesh_world(const MeshData& md, const mat4f& world,
                                   const float3& rayOriginWS, const float3& rayDirWS)
{
    PickingRegistry::Hit result{ {}, -1, BVH_FAR };
    const uint32_t triTotal = (uint32_t)md.indices.size() / 3;
    if (triTotal == 0) return result;

    // Prepare triangle data for tinybvh: array of 3 bvhvec4 per triangle.
    std::vector<tinybvh::bvhvec4> tris;
    tris.resize(size_t(triTotal) * 3);
    for (uint32_t t = 0; t < triTotal; ++t) {
        uint32_t base = t * 3;
        const float3& v0l = md.positions[ md.indices[base + 0] ];
        const float3& v1l = md.positions[ md.indices[base + 1] ];
        const float3& v2l = md.positions[ md.indices[base + 2] ];
        float3 v0 = transformPoint(world, v0l);
        float3 v1 = transformPoint(world, v1l);
        float3 v2 = transformPoint(world, v2l);
        tris[size_t(t) * 3 + 0] = tinybvh::bvhvec4(v0.x, v0.y, v0.z, 0.0f);
        tris[size_t(t) * 3 + 1] = tinybvh::bvhvec4(v1.x, v1.y, v1.z, 0.0f);
        tris[size_t(t) * 3 + 2] = tinybvh::bvhvec4(v2.x, v2.y, v2.z, 0.0f);
    }

    tinybvh::BVH bvh;
    bvh.Build(tris.data(), triTotal);

    tinybvh::Ray ray(
        tinybvh::bvhvec3(rayOriginWS.x, rayOriginWS.y, rayOriginWS.z),
        tinybvh::bvhvec3(rayDirWS.x, rayDirWS.y, rayDirWS.z),
        BVH_FAR
    );
    int32_t hitDist = bvh.Intersect(ray);
    if (hitDist >= 0 && ray.hit.prim < (uint32_t)triTotal) {
        result.triangle = int(ray.hit.prim);
        result.distance = ray.hit.t;
    }

    return result;
}

} // namespace filament::gltfio
