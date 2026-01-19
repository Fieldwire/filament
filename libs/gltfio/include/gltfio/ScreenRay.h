#pragma once

#include <filament/View.h>
#include <filament/Camera.h>
#include <filament/Viewport.h>
#include <math/mat4.h>
#include <math/vec3.h>
#include <math/norm.h>

namespace filament {
namespace gltfio {

// Computes a world-space ray (origin, direction) from screen pixel coordinates (sx, sy)
// using the exact logic from gltf_viewer onClick (lines 535–591).
// Returns true on success; false if the viewport is invalid.
inline bool computeScreenRay(filament::View* view, int sx, int sy,
                      filament::math::float3* outOrigin,
                      filament::math::float3* outDirection) {
    if (!view || !outOrigin || !outDirection) return false;
    filament::Camera* cam = &view->getCamera();
    if (!cam) return false;

    const filament::Viewport& vp = view->getViewport();
    if (vp.width <= 0 || vp.height <= 0) return false;

    double nx = (double(sx) / double(vp.width)) * 2.0 - 1.0;
    double ny = (double(sy) / double(vp.height)) * 2.0 - 1.0;

    using namespace filament::math;

    mat4 proj = cam->getProjectionMatrix();
    bool isPerspective = std::abs(proj[3][3]) < 1e-6;

    mat4 invProj = filament::Camera::inverseProjection(proj);
    mat4 viewM = cam->getViewMatrix();
    mat4 invView = inverse(viewM);

    if (isPerspective) {
        double4 clip{ nx, ny, -1.0, 1.0 }; // near plane
        double4 viewSpace = invProj * clip;
        float3 viewPoint = float3( (float)viewSpace.x, (float)viewSpace.y, (float)viewSpace.z );
        float3 dirView = normalize(viewPoint);
        float3 dirWorld = normalize( (invView * float4(dirView, 0)).xyz );
        *outOrigin = cam->getPosition();
        *outDirection = dirWorld;
    } else {
        double4 clip{ nx, ny, -1.0, 1.0 };
        double4 viewSpace = invProj * clip;
        float3 viewPoint = float3( (float)viewSpace.x, (float)viewSpace.y, (float)viewSpace.z );
        float3 worldPoint = (invView * float4(viewPoint, 1)).xyz;
        *outOrigin = worldPoint;
        *outDirection = normalize(cam->getForwardVector());
    }
    return true;
}

} // namespace gltfio
} // namespace filament

