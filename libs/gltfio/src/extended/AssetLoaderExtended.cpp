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

#include "AssetLoaderExtended.h"

#include "../DracoCache.h"
#include "../FFilamentAsset.h"
#include "../GltfEnums.h"
#include "../Utility.h"
#include "TangentsJobExtended.h"

#include <filament/BufferObject.h>

#include <geometry/SurfaceOrientation.h>

#include <utils/JobSystem.h>
#include <utils/Log.h>
#include <utils/Panic.h>

#include <cgltf.h>

#include <unordered_map>
#include <variant>

namespace filament::gltfio {

namespace {

constexpr int const GENERATED_0 = FFilamentAsset::ResourceInfoExtended::GENERATED_0_INDEX;
constexpr int const GENERATED_1 = FFilamentAsset::ResourceInfoExtended::GENERATED_1_INDEX;

using BufferSlot = AssetLoaderExtended::BufferSlot;
using BufferType = std::variant<short4*, ushort4*, float2*, float3*, float4*>;

struct AttributeHash {
    size_t operator()(Attribute const& key) const {
        size_t h1 = std::hash<uint64_t>{}((uint64_t) key.type);
        size_t h2 = std::hash<uint64_t>{}((uint64_t) key.index);
        return h1 ^ (h2 << 1);
    }
};

struct AttributeEqual {
    bool operator()(Attribute const& lhs, Attribute const& rhs) const {
        return lhs.type == rhs.type && lhs.index == rhs.index;
    }
};

using AttributesMap =
        std::unordered_map<Attribute, FilamentAttribute, AttributeHash, AttributeEqual>;

inline std::tuple<VertexBuffer::AttributeType, size_t, void*> getVertexBundle(
        VertexAttribute attrib, TangentsJobExtended::OutputParams& out) {
    VertexBuffer::AttributeType type;
    size_t byteCount = 0;
    void* data = nullptr;
    switch (attrib) {
        case VertexAttribute::POSITION:
            type = VertexBuffer::AttributeType::FLOAT3;
            byteCount = sizeof(float3);
            data = out.positions;
            out.positions = nullptr;
            break;
        case VertexAttribute::TANGENTS:
            type = VertexBuffer::AttributeType::SHORT4;
            byteCount = sizeof(short4);
            data = out.tbn;
            out.tbn = nullptr;
            break;
        case VertexAttribute::COLOR:
            type = VertexBuffer::AttributeType::FLOAT4;
            byteCount = sizeof(float4);
            data = out.colors;
            out.colors = nullptr;
            break;
        case VertexAttribute::UV0:
            type = VertexBuffer::AttributeType::FLOAT2;
            byteCount = sizeof(float2);
            data = out.uv0;
            out.uv0 = nullptr;
            break;
        case VertexAttribute::UV1:
            type = VertexBuffer::AttributeType::FLOAT2;
            byteCount = sizeof(float2);
            data = out.uv1;
            out.uv1 = nullptr;
            break;
        case VertexAttribute::BONE_INDICES:
            type = VertexBuffer::AttributeType::USHORT4;
            byteCount = sizeof(ushort4);
            data = out.joints;
            out.joints = nullptr;
            break;
        case VertexAttribute::BONE_WEIGHTS:
            type = VertexBuffer::AttributeType::FLOAT4;
            byteCount = sizeof(float4);
            data = out.weights;
            out.weights = nullptr;
            break;
        default:
            PANIC_POSTCONDITION("Unexpected vertex attribute %d", static_cast<int>(attrib));
    }
    return {type, byteCount, data};
}

// Returns the byte stride of a Filament VertexBuffer AttributeType.
inline size_t filamentAttrByteSize(VertexBuffer::AttributeType t) {
    switch (t) {
        case VertexBuffer::AttributeType::BYTE:
        case VertexBuffer::AttributeType::UBYTE:   return 1;
        case VertexBuffer::AttributeType::BYTE2:
        case VertexBuffer::AttributeType::UBYTE2:
        case VertexBuffer::AttributeType::SHORT:
        case VertexBuffer::AttributeType::USHORT:
        case VertexBuffer::AttributeType::HALF:    return 2;
        case VertexBuffer::AttributeType::BYTE3:
        case VertexBuffer::AttributeType::UBYTE3:  return 3;
        case VertexBuffer::AttributeType::BYTE4:
        case VertexBuffer::AttributeType::UBYTE4:
        case VertexBuffer::AttributeType::SHORT2:
        case VertexBuffer::AttributeType::USHORT2:
        case VertexBuffer::AttributeType::INT:
        case VertexBuffer::AttributeType::UINT:
        case VertexBuffer::AttributeType::FLOAT:
        case VertexBuffer::AttributeType::HALF2:   return 4;
        case VertexBuffer::AttributeType::SHORT3:
        case VertexBuffer::AttributeType::USHORT3:
        case VertexBuffer::AttributeType::HALF3:   return 6;
        case VertexBuffer::AttributeType::SHORT4:
        case VertexBuffer::AttributeType::USHORT4:
        case VertexBuffer::AttributeType::FLOAT2:
        case VertexBuffer::AttributeType::HALF4:   return 8;
        case VertexBuffer::AttributeType::FLOAT3:  return 12;
        case VertexBuffer::AttributeType::FLOAT4:  return 16;
        default: return 0;
    }
}

// Reads a vertex attribute from a decoded cgltf_accessor into a malloc'd buffer.
// Handles the has_meshopt_compression data path (reads from buffer_view->data).
// For types requiring conversion (e.g. SNORM8/UNORM8/SNORM16/UNORM16 → float),
// normalises component-wise. Returns {permitType, totalBytes, data} — caller owns the buffer.
struct AccessorData {
    VertexBuffer::AttributeType type;
    size_t totalBytes;
    void* data;
};

AccessorData makeZeroedAccessorData(cgltf_accessor const* accessor, size_t const vertexCount) {
    VertexBuffer::AttributeType permitType, actualType;
    getElementType(accessor->type, accessor->component_type, &permitType, &actualType);
    size_t const totalBytes = filamentAttrByteSize(permitType) * vertexCount;
    void* data = calloc(totalBytes, 1);
    return {permitType, totalBytes, data};
}

AccessorData readAccessorData(cgltf_accessor const* accessor, size_t const vertexCount) {
    assert_invariant(accessor);
    assert_invariant(accessor->count == vertexCount);

    if (!accessor->buffer_view) {
        utils::slog.w << "Missing buffer_view for accessor; using zeroed fallback data."
                      << utils::io::endl;
        return makeZeroedAccessorData(accessor, vertexCount);
    }

    VertexBuffer::AttributeType permitType, actualType;
    getElementType(accessor->type, accessor->component_type, &permitType, &actualType);

    bool const needsConversion = (permitType != actualType);
    size_t const outElemSize = filamentAttrByteSize(permitType);
    size_t const totalBytes = outElemSize * vertexCount;
    auto* out = static_cast<uint8_t*>(malloc(totalBytes));

    // Get the pointer to the decoded accessor data, respecting the meshopt path.
    uint8_t const* src = nullptr;
    if (accessor->buffer_view->has_meshopt_compression) {
        src = static_cast<uint8_t const*>(accessor->buffer_view->data);
        if (!src) {
            free(out);
            utils::slog.w << "Missing meshopt-decoded buffer_view->data for accessor; using "
                             "zeroed fallback data."
                          << utils::io::endl;
            return makeZeroedAccessorData(accessor, vertexCount);
        }
        src += accessor->offset;
    } else {
        auto const* buffer = accessor->buffer_view->buffer;
        src = buffer ? static_cast<uint8_t const*>(buffer->data) : nullptr;
        if (!src) {
            free(out);
            utils::slog.w << "Missing buffer->data for accessor; using zeroed fallback data."
                          << utils::io::endl;
            return makeZeroedAccessorData(accessor, vertexCount);
        }
        src += utility::computeBindingOffset(accessor);
    }

    size_t const compCount = cgltf_num_components(accessor->type);
    size_t const srcStride = accessor->stride > 0 ? accessor->stride
                                                   : compCount * filamentAttrByteSize(actualType);

    if (!needsConversion) {
        // Raw stride-aware copy: src and dest have the same element format.
        if (srcStride == outElemSize) {
            memcpy(out, src, totalBytes);
        } else {
            for (size_t i = 0; i < vertexCount; i++) {
                memcpy(out + i * outElemSize, src + i * srcStride, outElemSize);
            }
        }
    } else {
        // Integer → float conversion: SNORM8/UNORM8/SNORM16/UNORM16 → float.
        // The output permitType is always a FLOAT* type in these cases.
        float* outF = reinterpret_cast<float*>(out);
        for (size_t i = 0; i < vertexCount; i++) {
            uint8_t const* elem = src + i * srcStride;
            for (size_t c = 0; c < compCount; c++) {
                float v = 0.0f;
                switch (accessor->component_type) {
                    case cgltf_component_type_r_8: {
                        int8_t raw;
                        memcpy(&raw, elem + c, 1);
                        v = raw < -127 ? -1.0f : raw / 127.0f;
                        break;
                    }
                    case cgltf_component_type_r_8u: {
                        uint8_t raw;
                        memcpy(&raw, elem + c, 1);
                        v = raw / 255.0f;
                        break;
                    }
                    case cgltf_component_type_r_16: {
                        int16_t raw;
                        memcpy(&raw, elem + c * 2, 2);
                        v = raw < -32767 ? -1.0f : raw / 32767.0f;
                        break;
                    }
                    case cgltf_component_type_r_16u: {
                        uint16_t raw;
                        memcpy(&raw, elem + c * 2, 2);
                        v = raw / 65535.0f;
                        break;
                    }
                    default:
                        break;
                }
                outF[i * compCount + c] = v;
            }
        }
    }

    return {permitType, totalBytes, out};
}

// Builds tangent-space quaternions from decoded normal / tangent data using SurfaceOrientation.
// Returns nullptr on failure; caller owns returned memory (free()).
short4* buildFallbackTangentQuats(const cgltf_primitive* prim, size_t const vertexCount) {
    cgltf_accessor const* normalAccessor = nullptr;
    cgltf_accessor const* tangentAccessor = nullptr;

    for (cgltf_size i = 0; i < prim->attributes_count; i++) {
        auto const& attr = prim->attributes[i];
        if (!normalAccessor && attr.type == cgltf_attribute_type_normal) {
            normalAccessor = attr.data;
        }
        if (!tangentAccessor && attr.type == cgltf_attribute_type_tangent) {
            tangentAccessor = attr.data;
        }
    }

    if (!normalAccessor) {
        return nullptr;
    }

    AccessorData normalData = readAccessorData(normalAccessor, vertexCount);
    if (!normalData.data || normalData.type != VertexBuffer::AttributeType::FLOAT3) {
        if (normalData.data) {
            free(normalData.data);
        }
        return nullptr;
    }

    AccessorData tangentData{
            .type = VertexBuffer::AttributeType::FLOAT4,
            .totalBytes = 0,
            .data = nullptr
    };
    if (tangentAccessor) {
        tangentData = readAccessorData(tangentAccessor, vertexCount);
    }

    geometry::SurfaceOrientation::Builder sob;
    sob.vertexCount(vertexCount);
    sob.normals(static_cast<float3*>(normalData.data));

    if (tangentData.data && tangentData.type == VertexBuffer::AttributeType::FLOAT4) {
        sob.tangents(static_cast<float4*>(tangentData.data));
    }

    std::unique_ptr<geometry::SurfaceOrientation> helper(sob.build());
    short4* quats = nullptr;
    if (helper) {
        quats = static_cast<short4*>(malloc(sizeof(short4) * vertexCount));
        helper->getQuats(quats, vertexCount);
    }

    free(normalData.data);
    if (tangentData.data) {
        free(tangentData.data);
    }

    return quats;
}

// This will run the jobs to create tangent spaces if necessary, or simply forward the data if the
// input does not require processing. The output is a list of buffers that will be uploaded in the
// ResourceLoader.
std::vector<BufferSlot> computeGeometries(cgltf_primitive const* prim, uint8_t const jobType,
        AttributesMap const& attributesMap, std::vector<int> const& morphTargets, UvMap const& uvmap,
        filament::Engine* engine) {

    bool const isUnlit = prim->material ? prim->material->unlit : false;

    using Params = TangentsJobExtended::Params;

    std::unordered_map<int, Params> jobs;
    auto getJob = [&jobs](int key) -> Params& {
        return jobs.try_emplace(key).first->second;
    };

    // Create a job description for each triangle-based primitive.
    // Collect all TANGENT vertex attribute slots that need to be populated.
    if ((jobType & VERTEX_JOB) != 0) {
        auto& job = getJob(TangentsJobExtended::kMorphTargetUnused);
        job.in = {
                .prim = prim,
                .uvmap = uvmap,
        };
        job.jobType |= VERTEX_JOB;
    }
    if ((jobType & INDEX_JOB) != 0) {
        auto& job = getJob(TangentsJobExtended::kMorphTargetUnused);
        job.in = {
                .prim = prim,
                .uvmap = uvmap,
        };
        job.jobType |= INDEX_JOB;
    }
    for (auto const target: morphTargets) {
        auto& job = getJob(target);
        job.jobType = MORPH_TARGET_JOB;
        job.in = {
                .prim = prim,
                .morphTargetIndex = target,
        };
    }

    utils::JobSystem& js = engine->getJobSystem();
    utils::JobSystem::Job* parent = js.createJob();
    for (auto& [key, params]: jobs) {
        js.run(utils::jobs::createJob(js, parent,
                [pptr = &params] { TangentsJobExtended::run(pptr); }));
    }
    js.runAndWait(parent);

    std::vector<BufferSlot> slots;

    struct MorphTargetOut {
        int morphTarget;
        float3* positions;
        short4* tbn;
        size_t vertexCount;
    };
    std::vector<MorphTargetOut> morphTargetOuts;

    for (auto& [key, params]: jobs) {
        uint8_t const jobType = params.jobType;
        TangentsJobExtended::OutputParams& out = params.out;
        size_t const vertexCount = out.vertexCount;

        if ((jobType & VERTEX_JOB) != 0) {
            auto vertexBufferBuilder =
                    VertexBuffer::Builder().enableBufferObjects().vertexCount(vertexCount);

            std::vector<BufferSlot> vslots;
            bool slottedTangent = false;
            int maxSlot = 0;
            for (auto [cgltfAttr, filamentAttr]: attributesMap) {
                auto const [cattr, expectedIndex] = cgltfAttr;
                auto const [vattr, slot] = filamentAttr;
                auto const [type, byteCount, data] = getVertexBundle(vattr, out);

                vertexBufferBuilder.attribute(vattr, slot, type);

                // Here we generate data if needed.
                if (expectedIndex == GENERATED_0 || expectedIndex == GENERATED_1) {
                    // We should free `data` here because it's not being passed on to ResourceLoader.
                    if (data) {
                        free(data);
                    }

                    size_t const requiredSize = byteCount * vertexCount;
                    auto gendata = (uint8_t*) malloc(requiredSize);

                    if (vattr == filament::VertexAttribute::COLOR) {
                        // Assume white as the default if colors need to be generated.
                        float4* dataf = (float4*) gendata;
                        for (size_t i = 0; i < vertexCount; ++i) {
                            dataf[i] = float4(1.0, 1.0, 1.0, 1.0f);
                        }
                    } else {
                        memset(gendata, 0xff, requiredSize);
                    }

                    vslots.push_back({
                        .slot = slot,
                        .sizeInBytes = requiredSize,
                        .data = gendata,
                    });
                } else {
                    // Note that normalization is not necessary because we always convert the input.
                    if (vattr == filament::VertexAttribute::TANGENTS) {
                        vertexBufferBuilder.normalized(vattr);
                        slottedTangent = true;
                    }
                    vslots.push_back({
                        .slot = slot,
                        .sizeInBytes = byteCount * vertexCount,
                        .data = data,
                    });
                }
                maxSlot = std::max(maxSlot, slot);
            }

            // Tangent is always computed for lit.
            if (!slottedTangent && !isUnlit) {
                auto const slot = maxSlot + 1;
                auto const vattr = filament::VertexAttribute::TANGENTS;
                auto const [type, byteCount, data] = getVertexBundle(vattr, out);
                vertexBufferBuilder.attribute(vattr, slot, type);
                vertexBufferBuilder.normalized(vattr);
                vslots.push_back({
                        .slot = slot,
                        .sizeInBytes = byteCount * vertexCount,
                        .data = data,
                });
            }

            assert_invariant(!vslots.empty());
            vertexBufferBuilder.bufferCount(vslots.size());
            auto vertexBuffer = vertexBufferBuilder.build(*engine);
            std::for_each(vslots.begin(), vslots.end(),
                    [vertexBuffer](BufferSlot& slot) { slot.vertices = vertexBuffer; });
            slots.insert(slots.end(), vslots.begin(), vslots.end());
        }

        // When VERTEX_JOB is off (either mGenerateNormals=false, or the primitive has quantized
        // attribute types that TangentsJobExtended cannot process), no VertexBuffer is produced
        // by the tangent job. We build one here using real decoded accessor data for all
        // attributes except TANGENTS, which gets an identity quaternion placeholder.
        // Result: correct geometry and UV mapping; flat shading (lighting incorrect).
        if ((jobType & VERTEX_JOB) == 0 && (jobType & INDEX_JOB) != 0 && !attributesMap.empty()) {
            auto vertexBufferBuilder =
                    VertexBuffer::Builder().enableBufferObjects().vertexCount(vertexCount);

            std::vector<BufferSlot> vslots;
            bool slottedTangent = false;
            int maxSlot = 0;
            for (auto [cgltfAttr, filamentAttr] : attributesMap) {
                auto const [cattr_type, cattr_index] = cgltfAttr;
                auto const [vattr, slot] = filamentAttr;
                VertexBuffer::AttributeType type;
                size_t requiredSize;
                void* data;

                if (cattr_index == GENERATED_0 || cattr_index == GENERATED_1) {
                    // Synthetic attribute with no real accessor — generate placeholder data,
                    // matching the VERTEX_JOB path behaviour for generated attributes.
                    size_t attrByteCount;
                    if (vattr == VertexAttribute::COLOR) {
                        type = VertexBuffer::AttributeType::FLOAT4;
                        attrByteCount = sizeof(float4);
                    } else {
                        type = VertexBuffer::AttributeType::FLOAT2;
                        attrByteCount = sizeof(float2);
                    }
                    requiredSize = attrByteCount * vertexCount;
                    auto* genData = static_cast<uint8_t*>(malloc(requiredSize));
                    if (vattr == VertexAttribute::COLOR) {
                        float4* dataf = (float4*) genData;
                        for (size_t i = 0; i < vertexCount; ++i) {
                            dataf[i] = float4(1.0, 1.0, 1.0, 1.0f);
                        }
                    } else {
                        memset(genData, 0xff, requiredSize);
                    }
                    data = genData;

                } else if (vattr == VertexAttribute::TANGENTS) {
                    // Tangent job is disabled for this primitive, but we can still derive
                    // tangent-space quats from decoded normals/tangents.
                    // If derivation fails, fall back to identity quats (flat shading).
                    type = VertexBuffer::AttributeType::SHORT4;
                    vertexBufferBuilder.normalized(vattr);
                    slottedTangent = true;
                    requiredSize = sizeof(short4) * vertexCount;
                    short4* tbnData = buildFallbackTangentQuats(prim, vertexCount);
                    if (!tbnData) {
                        tbnData = static_cast<short4*>(malloc(requiredSize));
                        for (size_t i = 0; i < vertexCount; ++i) {
                            tbnData[i] = short4{0, 0, 0, 0x7FFF};
                        }
                    }
                    data = tbnData;

                } else {
                    // Real attribute — look up the cgltf_accessor and read decoded data.
                    // readAccessorData handles the has_meshopt_compression path and any
                    // integer→float type conversions required by getElementType.
                    cgltf_accessor const* accessor = nullptr;
                    for (cgltf_size i = 0; i < prim->attributes_count; i++) {
                        if (prim->attributes[i].type == cattr_type &&
                                prim->attributes[i].index == cattr_index) {
                            accessor = prim->attributes[i].data;
                            break;
                        }
                    }
                    assert_invariant(accessor);
                    auto result = readAccessorData(accessor, vertexCount);
                    type = result.type;
                    requiredSize = result.totalBytes;
                    data = result.data;
                }

                vertexBufferBuilder.attribute(vattr, slot, type);
                vslots.push_back({
                    .slot = slot,
                    .sizeInBytes = requiredSize,
                    .data = data,
                });
                maxSlot = std::max(maxSlot, slot);
            }
            // For lit materials, always include a tangent slot if not already present.
            if (!slottedTangent && !isUnlit) {
                auto const slot = maxSlot + 1;
                vertexBufferBuilder.attribute(VertexAttribute::TANGENTS, slot,
                        VertexBuffer::AttributeType::SHORT4);
                vertexBufferBuilder.normalized(VertexAttribute::TANGENTS);
                size_t const requiredSize = sizeof(short4) * vertexCount;
                auto* tbnData = static_cast<short4*>(malloc(requiredSize));
                for (size_t i = 0; i < vertexCount; ++i) {
                    tbnData[i] = short4{0, 0, 0, 0x7FFF};
                }
                vslots.push_back({
                    .slot = slot,
                    .sizeInBytes = requiredSize,
                    .data = tbnData,
                });
            }
            assert_invariant(!vslots.empty());
            vertexBufferBuilder.bufferCount(vslots.size());
            auto vertexBuffer = vertexBufferBuilder.build(*engine);
            std::for_each(vslots.begin(), vslots.end(),
                    [vertexBuffer](BufferSlot& slot) { slot.vertices = vertexBuffer; });
            slots.insert(slots.end(), vslots.begin(), vslots.end());
        }

        if ((jobType & INDEX_JOB) != 0) {
            auto indexBuffer = IndexBuffer::Builder()
                                       .indexCount(out.triangleCount * 3)
                                       .bufferType(IndexBuffer::IndexType::UINT)
                                       .build(*engine);

            slots.push_back({
                    .indices = indexBuffer,
                    .sizeInBytes = out.triangleCount * 3 * 4,
                    .data = out.triangles,
            });
            out.triangles = nullptr;
        }
        if ((jobType & MORPH_TARGET_JOB) != 0) {
            morphTargetOuts.push_back({
                    .morphTarget = params.in.morphTargetIndex,
                    .positions = out.positions,
                    .tbn = out.tbn,
                    .vertexCount = vertexCount,
            });
            out.positions = nullptr;
            out.tbn = nullptr;
        }

        // We should have passed ownership of all allocation to other parties.
        assert_invariant(out.isEmpty());
    }

    if (!morphTargets.empty()) {
        UTILS_UNUSED_IN_RELEASE
        auto const vertexCount = morphTargetOuts[0].vertexCount;
        for (auto target: morphTargetOuts) {
            assert_invariant(target.vertexCount == vertexCount);
            slots.push_back({
                    .offset = 0xdeadbeef,
                    .slot = target.morphTarget,
                    .targetData = {
                            .tbn = target.tbn,
                            .positions = target.positions,
                    }});
        }
    }
    return slots;
}

} // anonymous namespace

// The first portion of this function prepares the computation of geometries associated with one
// cgltf primitive by transforming types into Filament associated (or gltfio internal) types. If the
// input mesh is meshopt compressed or is in the Draco format, then it will be first transformed
// into the uncompressed version and then the geometries (tangents etc) will be computed.
bool AssetLoaderExtended::createPrimitive(Input* input, Output* out,
        std::vector<BufferSlot>& outSlots) {
    auto gltf = input->gltf;
    auto prim = input->prim;
    auto name = input->name;

    bool const isUnlit = prim->material ? prim->material->unlit : false;
    uint8_t jobType = 0;

    // In glTF, each primitive may or may not have an index buffer.
    const cgltf_accessor* indexAccessor = prim->indices;
    if (indexAccessor || prim->attributes_count > 0) {
        IndexBuffer::IndexType indexType;
        if (indexAccessor && !getIndexType(indexAccessor->component_type, &indexType)) {
            utils::slog.e << "Unrecognized index type in " << name << utils::io::endl;
            return false;
        }
        jobType |= INDEX_JOB;
    }

    // Gate VERTEX_JOB on both the caller-supplied flag and per-primitive attribute type detection.
    // TangentsJobExtended::unpack() only handles r_32f, r_8u, and r_16u component types.
    // Packed GLBs from gltfpack use SNORM8 (r_8, e.g. oct-filtered normals) and SNORM16
    // (r_16, e.g. KHR_mesh_quantization positions), which would hit PANIC_POSTCONDITION in
    // unpack(). Checking per-primitive is more precise than a global flag: a single glTF can
    // have some primitives with standard float attributes and others with quantized attributes.
    if (mGenerateNormals) {
        bool canRunVertexJob = true;
        for (cgltf_size i = 0; i < prim->attributes_count && canRunVertexJob; i++) {
            cgltf_component_type const ct = prim->attributes[i].data->component_type;
            if (ct != cgltf_component_type_r_32f &&
                ct != cgltf_component_type_r_8u &&
                ct != cgltf_component_type_r_16u) {
                canRunVertexJob = false;
            }
        }
        if (canRunVertexJob) {
            jobType |= VERTEX_JOB;
        }
    }

    AttributesMap attributesMap;
    bool hasUv0 = false, hasUv1 = false, hasVertexColor = false, hasNormals = false;
    int slotCount = 0;

    for (cgltf_size aindex = 0; aindex < prim->attributes_count; aindex++) {
        cgltf_attribute const attribute = prim->attributes[aindex];
        int const index = attribute.index;
        cgltf_attribute_type const atype = attribute.type;
        cgltf_accessor const* accessor = attribute.data;

        Attribute const cattr{atype, index};

        // At a minimum, surface orientation requires normals to be present in the source data.
        // Here we re-purpose the normals slot to point to the quats that get computed later.
        if (atype == cgltf_attribute_type_normal) {
            if (isUnlit) continue;
            if (!hasNormals) {
                FilamentAttribute const fattr { VertexAttribute::TANGENTS, slotCount++ };
                hasNormals = true;
                attributesMap[cattr] = fattr;
            }
            continue;
        }

        if (atype == cgltf_attribute_type_tangent) {
            if (isUnlit) continue;
            if (!hasNormals) {
                FilamentAttribute const fattr { VertexAttribute::TANGENTS, slotCount++ };
                hasNormals = true;
                attributesMap[cattr] = fattr;
            }
            continue;
        }

        // Translate the cgltf attribute enum into a Filament enum.
        VertexAttribute semantic;
        if (!getVertexAttrType(atype, &semantic)) {
            utils::slog.e << "Unrecognized vertex semantic in " << name << utils::io::endl;
            return false;
        }
        if (atype == cgltf_attribute_type_weights && index > 0) {
            utils::slog.e << "Too many bone weights in " << name << utils::io::endl;
            continue;
        }
        if (atype == cgltf_attribute_type_joints && index > 0) {
            utils::slog.e << "Too many joints in " << name << utils::io::endl;
            continue;
        }
        if (atype == cgltf_attribute_type_texcoord) {
            if (index >= UvMapSize) {
                utils::slog.e << "Too many texture coordinate sets in " << name << utils::io::endl;
                continue;
            }
            UvSet uvset = out->uvmap[index];
            switch (uvset) {
                case UV0:
                    semantic = VertexAttribute::UV0;
                    hasUv0 = true;
                    break;
                case UV1:
                    semantic = VertexAttribute::UV1;
                    hasUv1 = true;
                    break;
                case UNUSED:
                    // If we have a free slot, then include this unused UV set in the VertexBuffer.
                    // This allows clients to swap the glTF material with a custom material.
                    if (!hasUv0 && getNumUvSets(out->uvmap) == 0) {
                        semantic = VertexAttribute::UV0;
                        hasUv0 = true;
                        break;
                    }

                    // If there are no free slots then drop this unused texture coordinate set.
                    // This should not print an error or warning because the glTF spec stipulates an
                    // order of degradation for gracefully dropping UV sets. We implement this in
                    // constrainMaterial in MaterialProvider.
                    continue;
            }
        }

        if (atype == cgltf_attribute_type_color) {
            hasVertexColor = true;
        }

        // The positions accessor is required to have min/max properties, use them to expand
        // the bounding box for this primitive.
        if (atype == cgltf_attribute_type_position) {
            const float* minp = &accessor->min[0];
            const float* maxp = &accessor->max[0];
            out->aabb.min = min(out->aabb.min, float3(minp[0], minp[1], minp[2]));
            out->aabb.max = max(out->aabb.max, float3(maxp[0], maxp[1], maxp[2]));
        }

        if (VertexBuffer::AttributeType fatype, actualType;
                !getElementType(accessor->type, accessor->component_type, &fatype, &actualType)) {
            utils::slog.e << "Unsupported accessor type in " << name << utils::io::endl;
            return false;
        }

        attributesMap[cattr] = { semantic, slotCount++ };

        if (accessor->count == 0) {
            utils::slog.e << "Empty vertex buffer in " << name << utils::io::endl;
            return false;
        }
    }

    cgltf_size targetsCount = prim->targets_count;
    if (targetsCount > MAX_MORPH_TARGETS) {
        utils::slog.w << "WARNING: Exceeded max morph target count of " << MAX_MORPH_TARGETS
                      << utils::io::endl;
        targetsCount = MAX_MORPH_TARGETS;
    }

    // A set of morph targets to generate tangents for.
    std::vector<int> morphTargets;

    Aabb const baseAabb(out->aabb);
    for (cgltf_size targetIndex = 0; targetIndex < targetsCount; targetIndex++) {
        bool morphTargetHasNormals = false;
        cgltf_morph_target const& target = prim->targets[targetIndex];
        for (cgltf_size aindex = 0; aindex < target.attributes_count; aindex++) {
            cgltf_attribute const& attribute = target.attributes[aindex];
            cgltf_accessor const* accessor = attribute.data;
            cgltf_attribute_type const atype = attribute.type;

            if (atype != cgltf_attribute_type_position && atype != cgltf_attribute_type_normal &&
                    atype != cgltf_attribute_type_tangent) {
                utils::slog.e << "Only positions, normals, and tangents can be morphed."
                              << " type=" << static_cast<int>(atype) << utils::io::endl;
                return false;
            }

            if (VertexBuffer::AttributeType fatype, actualType; !getElementType(accessor->type,
                        accessor->component_type, &fatype, &actualType)) {
                utils::slog.e << "Unsupported accessor type in " << name << utils::io::endl;
                return false;
            }

            if (atype == cgltf_attribute_type_position && accessor->has_min && accessor->has_max) {
                Aabb targetAabb(baseAabb);
                float const* minp = &accessor->min[0];
                float const* maxp = &accessor->max[0];

                // We assume that the range of morph target weight is [0, 1].
                targetAabb.min += float3(minp[0], minp[1], minp[2]);
                targetAabb.max += float3(maxp[0], maxp[1], maxp[2]);

                out->aabb.min = min(out->aabb.min, targetAabb.min);
                out->aabb.max = max(out->aabb.max, targetAabb.max);
            }

            if (atype == cgltf_attribute_type_tangent) {
                morphTargetHasNormals = true;
                morphTargets.push_back(targetIndex);
            }
        }
        // Generate flat normals if necessary.
        if (!morphTargetHasNormals && prim->material && !prim->material->unlit) {
            morphTargets.push_back(targetIndex);
        }
    }

    // We provide a single dummy buffer (filled with 0xff) for all unfulfilled vertex requirements.
    // The color data should be a sequence of normalized UBYTE4, so dummy UVs are USHORT2 to make
    // the sizes match.
    if (mMaterials.needsDummyData(VertexAttribute::UV0) && !hasUv0) {
        attributesMap[{cgltf_attribute_type_texcoord, GENERATED_0}] = {VertexAttribute::UV0,
                slotCount++};
    }

    if (mMaterials.needsDummyData(VertexAttribute::UV1) && !hasUv1) {
        attributesMap[{cgltf_attribute_type_texcoord, GENERATED_1}] = {VertexAttribute::UV1,
                slotCount++};
    }

    if (mMaterials.needsDummyData(VertexAttribute::COLOR) && !hasVertexColor) {
        attributesMap[{cgltf_attribute_type_color, GENERATED_0}] = {VertexAttribute::COLOR,
                slotCount++};
    }

    int numUvSets = getNumUvSets(out->uvmap);
    if (!hasUv0 && numUvSets > 0) {
        attributesMap[{cgltf_attribute_type_texcoord, GENERATED_0}] = {VertexAttribute::UV0,
                slotCount++};
    }

    if (!hasUv1 && numUvSets > 1) {
        utils::slog.w << "Missing UV1 data in " << name << utils::io::endl;
        attributesMap[{cgltf_attribute_type_texcoord, GENERATED_1}] = {VertexAttribute::UV1,
                slotCount++};
    }

    auto needsBufferLoad = [](cgltf_data const* data) {
        for (cgltf_size i = 0; i < data->buffers_count; i++) {
            if (data->buffers[i].size > 0 && !data->buffers[i].data) {
                return true;
            }
        }
        return false;
    };
    auto needsMeshoptDecode = [](cgltf_data const* data) {
        for (cgltf_size i = 0; i < data->buffer_views_count; i++) {
            auto const& bv = data->buffer_views[i];
            if (bv.has_meshopt_compression && !bv.data) {
                return true;
            }
        }
        return false;
    };

    bool const seenBefore = mDecodedCgltfDatas.find(gltf) != mDecodedCgltfDatas.end();
    bool const shouldLoadBuffers = !seenBefore || needsBufferLoad(gltf);
    bool const shouldDecodeMeshopt = !seenBefore || needsMeshoptDecode(gltf);

    if (shouldLoadBuffers) {
        if (!utility::loadCgltfBuffers(gltf, mGltfPath.c_str(), mUriDataCache)) {
            return false;
        }
    }
    if (shouldDecodeMeshopt) {
        utility::decodeMeshoptCompression(gltf);
    }
    mDecodedCgltfDatas.insert(gltf);

    utility::decodeDracoMeshes(gltf, prim, input->dracoCache);

    auto slots = computeGeometries(prim, jobType, attributesMap, morphTargets, out->uvmap, mEngine);

    out->slotIndices.resize(morphTargets.size());

    for (size_t i = 0; i < slots.size(); i++) {
        auto& slot = slots[i];
        if (slot.vertices) {
            assert_invariant(!out->vertices || out->vertices == slot.vertices);
            out->vertices = slot.vertices;
        }
        if (slot.indices) {
            assert_invariant(!out->indices || out->indices == slot.indices);
            out->indices = slot.indices;
        }
        if (slot.offset == 0xdeadbeef) {
            // we can't fill this here, unfortunately, so this is done in
            // FAssetLoader::createRenderable
            out->slotIndices[slot.slot] = outSlots.size() + i;
        }
    }

    outSlots.insert(outSlots.end(), slots.begin(), slots.end());
    return true;
}

}// namespace filament::gltfio
