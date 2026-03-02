/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <jni.h>

#include <gltfio/FilamentAsset.h>
#include <gltfio/PickingRegistry.h>

#include <filament/Engine.h>
#include <filament/View.h>

using namespace filament;
using namespace filament::math;
using namespace filament::gltfio;
using namespace utils;

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetRoot(JNIEnv*, jclass, jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return asset->getRoot().getId();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nPopRenderable(JNIEnv*, jclass,
        jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return asset->popRenderable().getId();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nPopRenderables(JNIEnv* env, jclass,
        jlong nativeAsset, jintArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    jsize available = env->GetArrayLength(result);
    Entity* entities = (Entity*) env->GetIntArrayElements(result, nullptr);
    size_t retval = asset->popRenderables(entities, available);
    env->ReleaseIntArrayElements(result, (jint*) entities, 0);
    return retval;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetEntityCount(JNIEnv*, jclass,
        jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return asset->getEntityCount();
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetEntities(JNIEnv* env, jclass,
        jlong nativeAsset, jintArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    jsize available = env->GetArrayLength(result);
    Entity* entities = (Entity*) env->GetIntArrayElements(result, nullptr);
    std::copy_n(asset->getEntities(),
            std::min(available, (jsize) asset->getEntityCount()), entities);
    env->ReleaseIntArrayElements(result, (jint*) entities, 0);
}

extern "C" JNIEXPORT jint
Java_com_google_android_filament_gltfio_FilamentAsset_nGetFirstEntityByName(JNIEnv* env, jclass,
        jlong nativeAsset, jstring name) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    const char* cname = env->GetStringUTFChars(name, nullptr);
    Entity result = asset->getFirstEntityByName(cname);
    env->ReleaseStringUTFChars(name, cname);
    return result.getId();
}

extern "C" JNIEXPORT jint
Java_com_google_android_filament_gltfio_FilamentAsset_nGetEntitiesByName(JNIEnv* env, jclass,
        jlong nativeAsset, jstring name, jintArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    const char* cname = env->GetStringUTFChars(name, nullptr);
    size_t numEntities = asset->getEntitiesByName(cname, nullptr, 0);
    if (result == nullptr) {
        env->ReleaseStringUTFChars(name, cname);
        return numEntities;
    }
    Entity* entities = (Entity*) env->GetIntArrayElements(result, nullptr);
    numEntities = asset->getEntitiesByName(cname, entities, env->GetArrayLength(result));
    env->ReleaseIntArrayElements(result, (jint*) entities, 0);
    env->ReleaseStringUTFChars(name, cname);
    return numEntities;
}

extern "C" JNIEXPORT jint
Java_com_google_android_filament_gltfio_FilamentAsset_nGetEntitiesByPrefix(JNIEnv* env, jclass,
        jlong nativeAsset, jstring prefix, jintArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    const char* cprefix = env->GetStringUTFChars(prefix, nullptr);
    size_t numEntities = asset->getEntitiesByPrefix(cprefix, nullptr, 0);
    if (result == nullptr) {
        env->ReleaseStringUTFChars(prefix, cprefix);
        return numEntities;
    }
    Entity* entities = (Entity*) env->GetIntArrayElements(result, nullptr);
    numEntities = asset->getEntitiesByPrefix(cprefix, entities, env->GetArrayLength(result));
    env->ReleaseIntArrayElements(result, (jint*) entities, 0);
    env->ReleaseStringUTFChars(prefix, cprefix);
    return numEntities;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetLightEntityCount(JNIEnv*, jclass,
        jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return asset->getLightEntityCount();
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetLightEntities(JNIEnv* env, jclass,
        jlong nativeAsset, jintArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    const jsize available = env->GetArrayLength(result);
    const size_t minCount = std::min(available, (jsize) asset->getLightEntityCount());
    if (minCount == 0) {
        return;
    }
    Entity* entities = (Entity*) env->GetIntArrayElements(result, nullptr);
    std::copy_n(asset->getLightEntities(), minCount, entities);
    env->ReleaseIntArrayElements(result, (jint*) entities, 0);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetRenderableEntityCount(JNIEnv*, jclass,
        jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return asset->getRenderableEntityCount();
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetRenderableEntities(JNIEnv* env, jclass,
        jlong nativeAsset, jintArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    const jsize available = env->GetArrayLength(result);
    const size_t minCount = std::min(available, (jsize) asset->getRenderableEntityCount());
    if (minCount == 0) {
        return;
    }
    Entity* entities = (Entity*) env->GetIntArrayElements(result, nullptr);
    std::copy_n(asset->getRenderableEntities(), minCount, entities);
    env->ReleaseIntArrayElements(result, (jint*) entities, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetCameraEntities(JNIEnv* env, jclass,
        jlong nativeAsset, jintArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    const jsize available = env->GetArrayLength(result);
    const size_t minCount = std::min(available, (jsize) asset->getCameraEntityCount());
    if (minCount == 0) {
        return;
    }
    Entity* entities = (Entity*) env->GetIntArrayElements(result, nullptr);
    std::copy_n(asset->getCameraEntities(), minCount, entities);
    env->ReleaseIntArrayElements(result, (jint*) entities, 0);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetCameraEntityCount(JNIEnv*, jclass,
        jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return asset->getCameraEntityCount();
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetBoundingBox(JNIEnv* env, jclass,
        jlong nativeAsset, jfloatArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    float* values = env->GetFloatArrayElements(result, nullptr);
    const filament::Aabb box = asset->getBoundingBox();
    const float3 center = box.center();
    const float3 extent = box.extent();
    values[0] = center.x;
    values[1] = center.y;
    values[2] = center.z;
    values[3] = extent.x;
    values[4] = extent.y;
    values[5] = extent.z;
    env->ReleaseFloatArrayElements(result, values, 0);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetTriangleCount(JNIEnv*, jclass,
        jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return (jlong) asset->getTriangleCount();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetName(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId) {
    Entity entity = Entity::import(entityId);
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    const char* val = asset->getName(entity);
    return val ? env->NewStringUTF(val) : nullptr;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetExtras(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId) {
    Entity entity = Entity::import(entityId);
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    const auto val = asset->getExtras(entity);
    return val ? env->NewStringUTF(val) : nullptr;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetInstance(JNIEnv* , jclass,
        jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return (jlong) asset->getInstance();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetResourceUriCount(JNIEnv*, jclass,
                                                                           jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    return (jint) asset->getResourceUriCount();
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetResourceUris(JNIEnv* env, jclass,
                                                                       jlong nativeAsset,
                                                                       jobjectArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    auto resourceUris = asset->getResourceUris();
    for (int i = 0; i < asset->getResourceUriCount(); ++i) {
        env->SetObjectArrayElement(result, (jsize) i, env->NewStringUTF(resourceUris[i]));
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetMorphTargetCount(JNIEnv*, jclass,
        jlong nativeAsset, jint entityId) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    Entity entity = Entity::import(entityId);
    return (jint) asset->getMorphTargetCountAt(entity);
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetMorphTargetNames(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId, jobjectArray result) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    Entity entity = Entity::import(entityId);
    for (int i = 0, n = asset->getMorphTargetCountAt(entity); i < n; ++i) {
        const char* name = asset->getMorphTargetNameAt(entity, i);
        env->SetObjectArrayElement(result, (jsize) i, env->NewStringUTF(name));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nReleaseSourceData(JNIEnv* env, jclass,
        jlong nativeAsset) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    asset->releaseSourceData();
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nPick(JNIEnv* env, jclass,
        jlong nativeAsset, jlong nativeView, jlong nativeEngine, jint entityId,
        jint screenX, jint screenY, jintArray skipRanges) {

    // Create result array: [entityId, triangleIndex, distance]
    jintArray result = env->NewIntArray(3);
    if (result == nullptr) {
        return nullptr;
    }

    // Initialize with "no hit" values
    jint noHitData[3] = {0, -1, 0};
    env->SetIntArrayRegion(result, 0, 3, noHitData);

    // Validate pointers
    if (nativeAsset == 0 || nativeView == 0 || nativeEngine == 0) {
        return result;
    }

    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    View* view = (View*) nativeView;
    Engine* engine = (Engine*) nativeEngine;

    PickingRegistry* pickingRegistry = asset->getPickingRegistry();
    if (pickingRegistry == nullptr) {
        return result;
    }

    Entity entity = Entity::import(entityId);
    auto& tcm = engine->getTransformManager();

    // Process skip ranges if provided
    const uint32_t* skipRangesPtr = nullptr;
    size_t skipRangeCount = 0;
    jint* skipRangesArray = nullptr;

    if (skipRanges != nullptr) {
        jsize arrayLength = env->GetArrayLength(skipRanges);
        if (arrayLength > 0 && (arrayLength % 2) == 0) {
            skipRangesArray = env->GetIntArrayElements(skipRanges, nullptr);
            if (skipRangesArray != nullptr) {
                skipRangesPtr = reinterpret_cast<const uint32_t*>(skipRangesArray);
                skipRangeCount = arrayLength / 2;
            }
        }
    }

    // Perform picking
    auto hit = pickingRegistry->pick(*view, tcm, entity, screenX, screenY,
                                     skipRangesPtr, skipRangeCount);

    // Release skip ranges array
    if (skipRangesArray != nullptr) {
        env->ReleaseIntArrayElements(skipRanges, skipRangesArray, JNI_ABORT);
    }

    // Fill result array
    jint resultData[3];
    resultData[0] = hit.entity.getId();
    resultData[1] = hit.triangleIndex;
    resultData[2] = *reinterpret_cast<jint*>(&hit.distance);
    env->SetIntArrayRegion(result, 0, 3, resultData);

    return result;
}

/**
 * Returns mesh data buffer pointers and sizes for an entity.
 * Returns a long array: [positionsPtr, positionsSize, indicesPtr, indicesSize, expandedIndicesPtr, expandedIndicesSize]
 * The pointers can be used to create direct ByteBuffers on the Java side.
 * The sizes are in bytes (element count * element size).
 * This avoids copying data - the ByteBuffers will directly reference C++ memory.
 *
 * IMPORTANT: The returned pointers are only valid while the PickingRegistry exists.
 * Do not cache the ByteBuffers beyond the lifetime of the FilamentAsset.
 */
extern "C" JNIEXPORT jlongArray JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetMeshDataInfo(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    PickingRegistry* registry = asset->getPickingRegistry();

    if (!registry) {
        return nullptr;
    }

    Entity entity = Entity::import(entityId);
    const auto& meshes = registry->getMeshes();
    auto it = meshes.find(entity);

    if (it == meshes.end()) {
        return nullptr;
    }

    const MeshData& meshData = it->second;

    // Return: [positionsPtr, positionsSize, indicesPtr, indicesSize, expandedIndicesPtr, expandedIndicesSize]
    jlongArray result = env->NewLongArray(6);
    if (result == nullptr) {
        return nullptr;
    }

    jlong data[6];
    data[0] = reinterpret_cast<jlong>(meshData.positions.data());
    data[1] = static_cast<jlong>(meshData.positions.size() * sizeof(float3));
    data[2] = reinterpret_cast<jlong>(meshData.indices.data());
    data[3] = static_cast<jlong>(meshData.indices.size() * sizeof(uint32_t));
    data[4] = reinterpret_cast<jlong>(meshData.expandedIndices.data());
    data[5] = static_cast<jlong>(meshData.expandedIndices.size() * sizeof(uint32_t));

    env->SetLongArrayRegion(result, 0, 6, data);
    return result;
}

/**
 * Helper method to create a direct ByteBuffer from a native pointer and capacity.
 */
extern "C" JNIEXPORT jobject JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nNewDirectByteBuffer(JNIEnv* env, jclass,
        jlong address, jint capacity) {
    if (address == 0 || capacity <= 0) {
        return nullptr;
    }
    return env->NewDirectByteBuffer(reinterpret_cast<void*>(address), capacity);
}

