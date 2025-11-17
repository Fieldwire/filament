/*
 * PickingUtils.cpp - utility helpers for glTF picking (mesh CPU data construction).
 */

#include <cgltf.h>
#include <gltfio/Picking.h>

#include <vector>

using namespace filament::math;

namespace filament::gltfio {

MeshData buildMeshDataForPicking(const cgltf_mesh* mesh) {
    MeshData meshData = {};
    if (!mesh) return meshData;
    uint32_t baseVertex = 0;
    for (cgltf_size p = 0; p < mesh->primitives_count; ++p) {
        const cgltf_primitive& srcPrim = mesh->primitives[p];
        if (srcPrim.type != cgltf_primitive_type_triangles) {
            continue; // skip non-triangle primitives
        }
        // Locate position accessor.
        const cgltf_accessor* positionAccessor = nullptr;
        for (cgltf_size a = 0; a < srcPrim.attributes_count; ++a) {
            if (srcPrim.attributes[a].type == cgltf_attribute_type_position) {
                positionAccessor = srcPrim.attributes[a].data;
                break;
            }
        }
        if (!positionAccessor || !positionAccessor->count) {
            continue; // no positions
        }
        // Determine component count and normalize to 3D.
        int numComponents = 0;
        switch (positionAccessor->type) {
            case cgltf_type_vec3: numComponents = 3; break;
            case cgltf_type_vec2: numComponents = 2; break; // promote z=0
            case cgltf_type_vec4: numComponents = 4; break; // ignore w
            case cgltf_type_scalar: numComponents = 1; break; // promote y,z=0
            default: numComponents = 3; break;
        }
        const cgltf_size count = positionAccessor->count;
        std::vector<float> tmp(count * numComponents);
        cgltf_size written = cgltf_accessor_unpack_floats(positionAccessor, tmp.data(), tmp.size());
        meshData.positions.reserve(meshData.positions.size() + count);
        if (written == tmp.size()) {
            for (cgltf_size i = 0; i < count; ++i) {
                float x = tmp[i * numComponents + 0];
                float y = (numComponents > 1) ? tmp[i * numComponents + 1] : 0.0f;
                float z = (numComponents > 2) ? tmp[i * numComponents + 2] : 0.0f;
                meshData.positions.emplace_back(x, y, z);
            }
        } else {
            float scratch[4];
            for (cgltf_size i = 0; i < count; ++i) {
                if (cgltf_accessor_read_float(positionAccessor, i, scratch, numComponents) == cgltf_result_success) {
                    float x = scratch[0];
                    float y = (numComponents > 1) ? scratch[1] : 0.0f;
                    float z = (numComponents > 2) ? scratch[2] : 0.0f;
                    meshData.positions.emplace_back(x, y, z);
                }
            }
        }
        // Indices: explicit or generated.
        if (srcPrim.indices && srcPrim.indices->count) {
            const cgltf_accessor* indexAccessor = srcPrim.indices;
            cgltf_size icount = indexAccessor->count;
            meshData.indices.reserve(meshData.indices.size() + icount);
            for (cgltf_size i = 0; i < icount; ++i) {
                cgltf_size value = cgltf_accessor_read_index(indexAccessor, i);
                meshData.indices.push_back((uint32_t) value + baseVertex);
            }
        } else {
            meshData.indices.reserve(meshData.indices.size() + count);
            for (cgltf_size i = 0; i < count; ++i) {
                meshData.indices.push_back((uint32_t) i + baseVertex);
            }
        }
        baseVertex += (uint32_t) count;
    }
    return meshData; // localBounds left empty for caller to fill
}

} // namespace filament::gltfio

