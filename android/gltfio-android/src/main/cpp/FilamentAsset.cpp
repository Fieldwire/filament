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
#include <iostream>
#include <android/log.h>

#define LOG_TAG "FilamentAsset"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#include <gltfio/FilamentAsset.h>
#include <gltfio/PickingRegistry.h>
#include <gltfio/TinyBVHPicking.h>
#include <gltfio/ScreenRay.h>
#include <gltfio/TriangleHiding.h>
#include <filament/TransformManager.h>
#include <filament/Engine.h>
#include <filament/View.h>
#include <filament/Camera.h>
#include <filament/Viewport.h> // Added to provide full definition of filament::Viewport
#include <math/mat4.h>
#include <gltfio/PickingUtils.h>

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

extern "C" JNIEXPORT jobject JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nRayPick(JNIEnv* env, jclass,
        jlong nativeAsset, jfloat ox, jfloat oy, jfloat oz, jfloat dx, jfloat dy, jfloat dz) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return nullptr;
    // Update world transforms for all renderables prior to picking for accuracy.
    Engine* engine = asset->getEngine();
    if (engine) {
        auto& tcm = engine->getTransformManager();
        size_t rc = asset->getRenderableEntityCount();
        const utils::Entity* renderables = asset->getRenderableEntities();
        if (renderables) {
            for (size_t i = 0; i < rc; ++i) {
                auto inst = tcm.getInstance(renderables[i]);
                if (inst) {
                    reg->updateTransform(renderables[i], tcm.getWorldTransform(inst));
                }
            }
        }
    }
    auto hit = reg->pick(float3{ox, oy, oz}, float3{dx, dy, dz});
    if (hit.entity.getId() == 0 || hit.triangle < 0) {
        return nullptr; // no intersection
    }
    jclass hitClass = env->FindClass("com/google/android/filament/gltfio/FilamentAsset$Hit");
    if (!hitClass) return nullptr; // class not found
    jmethodID ctor = env->GetMethodID(hitClass, "<init>", "(IIFFFF)V");
    if (!ctor) return nullptr; // constructor not found
    return env->NewObject(hitClass, ctor,
            (jint) hit.entity.getId(), (jint) hit.triangle,
            (jfloat) hit.distance, (jfloat) hit.bary.x, (jfloat) hit.bary.y, (jfloat) hit.bary.z);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nRayPickScreen(JNIEnv* env, jclass,
        jlong nativeAsset, jlong nativeView, jint sx, jint sy) {
    LOGD("[nRayPickScreen] sx=%d sy=%d", sx, sy);

    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    View* view = (View*) nativeView;
    if (!view) return nullptr;
    Camera* cam = &view->getCamera();

    // Update transforms before picking.
    size_t updatedCount = gltfio::updatePickingTransforms(asset);
    LOGD("[nRayPickScreen] updatedTransforms=%zu", updatedCount);

    // Build ray using shared helper to mirror desktop math.
    float3 rayOrigin;
    float3 rayDir;
    if (!gltfio::computeScreenRay(view, (int)sx, (int)sy, &rayOrigin, &rayDir)) {
        LOGD("[nRayPickScreen] invalid viewport or inputs");
        return nullptr;
    }
    LOGD("[nRayPickScreen] camPos=(%.3f,%.3f,%.3f) rayOrigin=(%.3f,%.3f,%.3f) rayDir=(%.3f,%.3f,%.3f)",
         cam->getPosition().x, cam->getPosition().y, cam->getPosition().z,
         rayOrigin.x, rayOrigin.y, rayOrigin.z,
         rayDir.x, rayDir.y, rayDir.z);

    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return nullptr;
    auto hit = reg->pick(rayOrigin, rayDir);
    if (hit.entity.getId() == 0 || hit.triangle < 0) {
        LOGD("[nRayPickScreen] No hit");
        return nullptr;
    }
    LOGD("[nRayPickScreen] Hit entity=%d tri=%d dist=%.3f",
         hit.entity.getId(), hit.triangle, hit.distance);

    jclass hitClass = env->FindClass("com/google/android/filament/gltfio/FilamentAsset$Hit");
    if (!hitClass) return nullptr;
    jmethodID ctor = env->GetMethodID(hitClass, "<init>", "(IIFFFF)V");
    if (!ctor) return nullptr;
    return env->NewObject(hitClass, ctor,
            (jint) hit.entity.getId(), (jint) hit.triangle,
            (jfloat) hit.distance, (jfloat) hit.bary.x, (jfloat) hit.bary.y, (jfloat) hit.bary.z);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nRayPickScreenSkippingRange(JNIEnv* env, jclass,
        jlong nativeAsset, jlong nativeView, jint sx, jint sy, jint startIdx, jint endIdx) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    View* view = (View*) nativeView;
    if (!view) return nullptr;

    // Update transforms similar to nRayPickScreen
    gltfio::updatePickingTransforms(asset);

    float3 rayOrigin;
    float3 rayDir;
    if (!gltfio::computeScreenRay(view, (int)sx, (int)sy, &rayOrigin, &rayDir)) {
        return nullptr;
    }

    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return nullptr;
    auto hit = reg->pickSkippingIndexRange(rayOrigin, rayDir, (uint32_t)startIdx, (uint32_t)endIdx);
    if (hit.entity.getId() == 0 || hit.triangle < 0) {
        return nullptr;
    }

    jclass hitClass = env->FindClass("com/google/android/filament/gltfio/FilamentAsset$Hit");
    if (!hitClass) return nullptr;
    jmethodID ctor = env->GetMethodID(hitClass, "<init>", "(IIFFFF)V");
    if (!ctor) return nullptr;
    return env->NewObject(hitClass, ctor,
            (jint) hit.entity.getId(), (jint) hit.triangle,
            (jfloat) hit.distance, (jfloat) hit.bary.x, (jfloat) hit.bary.y, (jfloat) hit.bary.z);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nRayPickScreenTinybvh(JNIEnv* env, jclass,
        jlong nativeAsset, jlong nativeView, jint sx, jint sy) {
    LOGD("[nRayPickScreen] sx=%d sy=%d", sx, sy);

    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    View* view = (View*) nativeView;
    if (!view) return nullptr;

    // Update transforms before picking.
    size_t updatedCount = gltfio::updatePickingTransforms(asset);
    LOGD("[nRayPickScreen] updatedTransforms=%zu", updatedCount);

    TinyBVHPickingRegistry* reg = asset->getTinyBVHPickingRegistry();
    if (!reg) return nullptr;

    int2 position = { sx, sy };
    auto hit_ptr = reg ? reg->pick(view, position, asset) : nullptr;

    if (!hitptr) {
        LOGD("[nRayPickScreen] No hitptr");
        return nullptr;
    }

    auto hit = *hitptr;

    if (hit.entity.getId() == 0 || hit.triangle < 0) {
        LOGD("[nRayPickScreen] No hit");
        return nullptr;
    }
    LOGD("[nRayPickScreen] Hit entity=%d tri=%d dist=%.3f",
         hit.entity.getId(), hit.triangle, hit.distance);

    jclass hitClass = env->FindClass("com/google/android/filament/gltfio/FilamentAsset$Hit");
    if (!hitClass) return nullptr;
    jmethodID ctor = env->GetMethodID(hitClass, "<init>", "(IIFFFF)V");
    if (!ctor) return nullptr;
    return env->NewObject(hitClass, ctor,
            (jint) hit.entity.getId(), (jint) hit.triangle,
            (jfloat) hit.distance, (jfloat) hit.bary.x, (jfloat) hit.bary.y, (jfloat) hit.bary.z);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nRayPickScreenSkippingRangeTinybvh(JNIEnv* env, jclass,
        jlong nativeAsset, jlong nativeView, jint sx, jint sy, jint startIdx, jint endIdx) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    View* view = (View*) nativeView;
    if (!view) return nullptr;

    TinyBVHPickingRegistry* reg = asset->getTinyBVHPickingRegistry();
    if (!reg) return nullptr;

    auto position = int2{ sx, sy };
    auto hit_ptr = reg->pickSkippingIndexRange(view, position, static_cast<uint32_t>(startIdx), static_cast<uint32_t>(endIdx));

    auto hit = *hit_ptr;

    if (hit.entity.getId() == 0 || hit.triangle < 0) {
        return nullptr;
    }

    jclass hitClass = env->FindClass("com/google/android/filament/gltfio/FilamentAsset$Hit");
    if (!hitClass) return nullptr;
    jmethodID ctor = env->GetMethodID(hitClass, "<init>", "(IIFFFF)V");
    if (!ctor) return nullptr;
    return env->NewObject(hitClass, ctor,
            (jint) hit.entity.getId(), (jint) hit.triangle,
            (jfloat) hit.distance, (jfloat) hit.bary.x, (jfloat) hit.bary.y, (jfloat) hit.bary.z);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetTriangleModelSpaceForHit(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId, jint triangleIndex) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset || triangleIndex < 0) return nullptr;
    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return nullptr;
    Entity e = Entity::import((uint32_t)entityId);
    auto* md = reg->getMesh(e);
    if (!md) return nullptr;

    uint32_t base = (uint32_t)triangleIndex;
    // Basic bounds checks to avoid OOB.
    if (!md->indices.size() || base + 2 >= md->indices.size()) return nullptr;
    uint32_t i0 = md->indices[base + 0];
    uint32_t i1 = md->indices[base + 1];
    uint32_t i2 = md->indices[base + 2];
    if (!md->positions.size() || i0 >= md->positions.size() || i1 >= md->positions.size() || i2 >= md->positions.size()) {
        return nullptr;
    }

    float3 v0 = md->positions[i0];
    float3 v1 = md->positions[i1];
    float3 v2 = md->positions[i2];

    jfloatArray out = env->NewFloatArray(9);
    if (!out) return nullptr;
    jfloat vals[9] = { v0.x, v0.y, v0.z, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z };
    env->SetFloatArrayRegion(out, 0, 9, vals);
    return out;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetIndicesSize(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return -1;
    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return -1;
    Entity e = Entity::import((uint32_t)entityId);
    auto* md = reg->getMesh(e);
    if (!md) return -1;
    return static_cast<jint>(md->indices.size());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetPositionsSize(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return -1;
    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return -1;
    Entity e = Entity::import((uint32_t)entityId);
    auto* md = reg->getMesh(e);
    if (!md) return -1;
    return static_cast<jint>(md->positions.size());
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetMeshIndices(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return nullptr;
    Entity e = Entity::import((uint32_t)entityId);
    auto* md = reg->getMesh(e);
    if (!md || md->indices.empty()) return nullptr;

    jintArray out = env->NewIntArray(static_cast<jint>(md->indices.size()));
    if (!out) return nullptr;

    std::vector<jint> jIndices;
    jIndices.reserve(md->indices.size());
    for (uint32_t idx : md->indices) {
        jIndices.push_back(static_cast<jint>(idx));
    }

    env->SetIntArrayRegion(out, 0, static_cast<jint>(jIndices.size()), jIndices.data());
    return out;
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nGetMeshPositions(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return nullptr;
    Entity e = Entity::import((uint32_t)entityId);
    auto* md = reg->getMesh(e);
    if (!md || md->positions.empty()) return nullptr;

    jfloatArray out = env->NewFloatArray(static_cast<jint>(md->positions.size() * 3));
    if (!out) return nullptr;

    std::vector<jfloat> jPositions;
    jPositions.reserve(md->positions.size() * 3);
    for (const float3& pos : md->positions) {
        jPositions.push_back(pos.x);
        jPositions.push_back(pos.y);
        jPositions.push_back(pos.z);
    }

    env->SetFloatArrayRegion(out, 0, static_cast<jint>(jPositions.size()), jPositions.data());
    return out;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nCreateTriangleHider(JNIEnv*, jclass,
        jlong nativeEngine) {
    Engine* engine = (Engine*) nativeEngine;
    if (!engine) return 0;
    return (jlong) new gltfio::TriangleHider(engine);
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nDestroyTriangleHider(JNIEnv*, jclass,
        jlong nativeHider) {
    gltfio::TriangleHider* hider = (gltfio::TriangleHider*) nativeHider;
    delete hider;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nHideTriangle(JNIEnv*, jclass,
        jlong nativeHider, jlong nativeAsset, jint entityId, jint triangleIndex) {
    gltfio::TriangleHider* hider = (gltfio::TriangleHider*) nativeHider;
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!hider || !asset) return false;

    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return false;

    Entity e = Entity::import((uint32_t)entityId);
    auto* md = reg->getMesh(e);
    if (!md) return false;

    return hider->hideTriangle(e, (uint32_t)triangleIndex, asset);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nHideTriangleWithoutCache(JNIEnv* env, jclass,
                                                                                jlong nativeAsset, jint entityId, jint triangleIndex) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return (jboolean) false;

    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return (jboolean) false;

    Entity e = Entity::import((uint32_t) entityId);
    auto* md = reg->getMesh(e);
    if (!md) return (jboolean) false;

    // Create a local TriangleHider for one-off hiding without cache
    Engine* engine = asset->getEngine();
    TriangleHider hider(engine);
    bool ok = hider.hideTriangleWithoutCache(e, (uint32_t) triangleIndex, md);
    return (jboolean) ok;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nHideVerticesInRangeWithoutCache(JNIEnv* env, jclass,
        jlong nativeAsset, jint entityId, jint startVertex, jint endVertex) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return (jboolean) false;

    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return (jboolean) false;

    Entity e = Entity::import((uint32_t) entityId);
    auto* md = reg->getMesh(e);
    if (!md) return (jboolean) false;

    // Create a local TriangleHider for one-off hiding without cache
    Engine* engine = asset->getEngine();
    TriangleHider hider(engine);
    bool ok = hider.hideVerticesInRangeWithoutCache(e, (uint32_t) startVertex, (uint32_t) endVertex, md);
    return (jboolean) ok;
}

// ============================================================================================
// TinyBVH-accelerated picking JNI methods
// ============================================================================================

extern "C" JNIEXPORT jobject JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nRayPickScreenTinyBVH(JNIEnv* env, jclass,
        jlong nativeAsset, jlong nativeView, jint sx, jint sy) {
    LOGD("[nRayPickScreenTinyBVH] sx=%d sy=%d", sx, sy);

    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    View* view = (View*) nativeView;
    if (!view) return nullptr;

    // Update transforms before picking
    gltfio::updatePickingTransforms(asset);

    // Build ray using shared helper
    float3 rayOrigin;
    float3 rayDir;
    if (!gltfio::computeScreenRay(view, (int)sx, (int)sy, &rayOrigin, &rayDir)) {
        LOGD("[nRayPickScreenTinyBVH] invalid viewport or inputs");
        return nullptr;
    }

    // Use TinyBVH picking registry if available
    // Note: You need to create a TinyBVHPickingRegistry and register meshes during asset loading
    // For now, this is a placeholder - you'd need to extend FilamentAsset to support tinybvh

    // Fallback to standard picking for demonstration
    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return nullptr;
    auto hit = reg->pick(rayOrigin, rayDir);

    if (hit.entity.getId() == 0 || hit.triangle < 0) {
        LOGD("[nRayPickScreenTinyBVH] No hit");
        return nullptr;
    }

    LOGD("[nRayPickScreenTinyBVH] Hit entity=%d tri=%d dist=%.3f",
         hit.entity.getId(), hit.triangle, hit.distance);

    jclass hitClass = env->FindClass("com/google/android/filament/gltfio/FilamentAsset$Hit");
    if (!hitClass) return nullptr;
    jmethodID ctor = env->GetMethodID(hitClass, "<init>", "(IIFFFF)V");
    if (!ctor) return nullptr;
    return env->NewObject(hitClass, ctor,
            (jint) hit.entity.getId(), (jint) hit.triangle,
            (jfloat) hit.distance, (jfloat) hit.bary.x, (jfloat) hit.bary.y, (jfloat) hit.bary.z);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_google_android_filament_gltfio_FilamentAsset_nRayPickScreenTinyBVHSkippingRange(JNIEnv* env, jclass,
        jlong nativeAsset, jlong nativeView, jint sx, jint sy, jint startIdx, jint endIdx) {
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    View* view = (View*) nativeView;
    if (!view) return nullptr;

    gltfio::updatePickingTransforms(asset);

    float3 rayOrigin;
    float3 rayDir;
    if (!gltfio::computeScreenRay(view, (int)sx, (int)sy, &rayOrigin, &rayDir)) {
        return nullptr;
    }

    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return nullptr;
    auto hit = reg->pickSkippingIndexRange(rayOrigin, rayDir, (uint32_t)startIdx, (uint32_t)endIdx);

    if (hit.entity.getId() == 0 || hit.triangle < 0) {
        return nullptr;
    }

    jclass hitClass = env->FindClass("com/google/android/filament/gltfio/FilamentAsset$Hit");
    if (!hitClass) return nullptr;
    jmethodID ctor = env->GetMethodID(hitClass, "<init>", "(IIFFFF)V");
    if (!ctor) return nullptr;
    return env->NewObject(hitClass, ctor,
            (jint) hit.entity.getId(), (jint) hit.triangle,
            (jfloat) hit.distance, (jfloat) hit.bary.x, (jfloat) hit.bary.y, (jfloat) hit.bary.z);
}

