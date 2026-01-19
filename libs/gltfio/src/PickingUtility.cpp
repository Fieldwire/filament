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

        // Locate all attribute accessors
        const cgltf_accessor* positionAccessor = nullptr;
        const cgltf_accessor* normalAccessor = nullptr;
        const cgltf_accessor* tangentAccessor = nullptr;
        const cgltf_accessor* uvAccessor = nullptr;
        const cgltf_accessor* colorAccessor = nullptr;

        for (cgltf_size a = 0; a < srcPrim.attributes_count; ++a) {
            const auto& attr = srcPrim.attributes[a];
            switch (attr.type) {
                case cgltf_attribute_type_position:
                    positionAccessor = attr.data;
                    break;
                case cgltf_attribute_type_normal:
                    normalAccessor = attr.data;
                    break;
                case cgltf_attribute_type_tangent:
                    tangentAccessor = attr.data;
                    break;
                case cgltf_attribute_type_texcoord:
                    if (attr.index == 0) { // Only capture TEXCOORD_0
                        uvAccessor = attr.data;
                    }
                    break;
                case cgltf_attribute_type_color:
                    if (attr.index == 0) { // Only capture COLOR_0
                        colorAccessor = attr.data;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!positionAccessor || !positionAccessor->count) {
            continue; // no positions
        }

        const cgltf_size count = positionAccessor->count;

        // Extract positions
        int numComponents = 0;
        switch (positionAccessor->type) {
            case cgltf_type_vec3: numComponents = 3; break;
            case cgltf_type_vec2: numComponents = 2; break;
            case cgltf_type_vec4: numComponents = 4; break;
            case cgltf_type_scalar: numComponents = 1; break;
            default: numComponents = 3; break;
        }
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

        // Extract normals
        if (normalAccessor && normalAccessor->type == cgltf_type_vec3) {
            meshData.hasNormals = true;
            meshData.normals.reserve(meshData.normals.size() + count);
            std::vector<float> normalData(count * 3);
            if (cgltf_accessor_unpack_floats(normalAccessor, normalData.data(), normalData.size()) == count * 3) {
                for (cgltf_size i = 0; i < count; ++i) {
                    meshData.normals.emplace_back(
                        normalData[i * 3 + 0],
                        normalData[i * 3 + 1],
                        normalData[i * 3 + 2]
                    );
                }
            } else {
                // Fallback: read one by one
                for (cgltf_size i = 0; i < count; ++i) {
                    float n[3] = {0.0f, 0.0f, 1.0f}; // default normal
                    cgltf_accessor_read_float(normalAccessor, i, n, 3);
                    meshData.normals.emplace_back(n[0], n[1], n[2]);
                }
            }
        }

        // Extract tangents
        if (tangentAccessor && tangentAccessor->type == cgltf_type_vec4) {
            meshData.hasTangents = true;
            meshData.tangents.reserve(meshData.tangents.size() + count);
            std::vector<float> tangentData(count * 4);
            if (cgltf_accessor_unpack_floats(tangentAccessor, tangentData.data(), tangentData.size()) == count * 4) {
                for (cgltf_size i = 0; i < count; ++i) {
                    meshData.tangents.emplace_back(
                        tangentData[i * 4 + 0],
                        tangentData[i * 4 + 1],
                        tangentData[i * 4 + 2],
                        tangentData[i * 4 + 3]
                    );
                }
            } else {
                for (cgltf_size i = 0; i < count; ++i) {
                    float t[4] = {1.0f, 0.0f, 0.0f, 1.0f}; // default tangent
                    cgltf_accessor_read_float(tangentAccessor, i, t, 4);
                    meshData.tangents.emplace_back(t[0], t[1], t[2], t[3]);
                }
            }
        }

        // Extract UVs
        if (uvAccessor && uvAccessor->type == cgltf_type_vec2) {
            meshData.hasUVs = true;
            meshData.uvs.reserve(meshData.uvs.size() + count);
            std::vector<float> uvData(count * 2);
            if (cgltf_accessor_unpack_floats(uvAccessor, uvData.data(), uvData.size()) == count * 2) {
                for (cgltf_size i = 0; i < count; ++i) {
                    meshData.uvs.emplace_back(
                        uvData[i * 2 + 0],
                        uvData[i * 2 + 1]
                    );
                }
            } else {
                for (cgltf_size i = 0; i < count; ++i) {
                    float uv[2] = {0.0f, 0.0f};
                    cgltf_accessor_read_float(uvAccessor, i, uv, 2);
                    meshData.uvs.emplace_back(uv[0], uv[1]);
                }
            }
        }

        // Extract colors
        if (colorAccessor) {
            meshData.hasColors = true;
            meshData.colors.reserve(meshData.colors.size() + count);
            int colorComponents = 0;
            switch (colorAccessor->type) {
                case cgltf_type_vec3: colorComponents = 3; break;
                case cgltf_type_vec4: colorComponents = 4; break;
                default: colorComponents = 4; break;
            }
            std::vector<float> colorData(count * colorComponents);
            if (cgltf_accessor_unpack_floats(colorAccessor, colorData.data(), colorData.size()) == count * colorComponents) {
                for (cgltf_size i = 0; i < count; ++i) {
                    float r = colorData[i * colorComponents + 0];
                    float g = (colorComponents > 1) ? colorData[i * colorComponents + 1] : r;
                    float b = (colorComponents > 2) ? colorData[i * colorComponents + 2] : r;
                    float a = (colorComponents > 3) ? colorData[i * colorComponents + 3] : 1.0f;
                    meshData.colors.emplace_back(r, g, b, a);
                }
            } else {
                for (cgltf_size i = 0; i < count; ++i) {
                    float c[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                    cgltf_accessor_read_float(colorAccessor, i, c, colorComponents);
                    float r = c[0];
                    float g = (colorComponents > 1) ? c[1] : r;
                    float b = (colorComponents > 2) ? c[2] : r;
                    float a = (colorComponents > 3) ? c[3] : 1.0f;
                    meshData.colors.emplace_back(r, g, b, a);
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

