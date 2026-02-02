#pragma once

#include <filament/View.h>
#include <math/vec3.h>

namespace filament {
namespace gltfio {

// Computes a world-space ray (origin, direction) from screen pixel coordinates (sx, sy)
// using the exact logic from gltf_viewer onClick (lines 535–591).
// Returns true on success; false if the viewport is invalid.
bool computeScreenRay(filament::View* view, int sx, int sy,
                      filament::math::float3* outOrigin,
                      filament::math::float3* outDirection);
std::pair<math::float3, math::float3> *computeScreenRay(View* view, math::int2 position);
} // namespace gltfio
} // namespace filament
