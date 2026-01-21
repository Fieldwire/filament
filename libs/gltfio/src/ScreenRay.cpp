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

#include <gltfio/ScreenRay.h>

#include <filament/Camera.h>
#include <filament/Viewport.h>
#include <math/mat4.h>
#include <math/vec3.h>
#include <math/vec4.h>
#include <math/norm.h>

#include <cmath>

namespace filament::gltfio {

bool computeScreenRay(filament::View* view, int sx, int sy,
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

} // namespace filament::gltfio

