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
#include <gltfio/Picking.h>
#include <filament/TransformManager.h>
#include <filament/Engine.h>
#include <filament/View.h>
#include <filament/Camera.h>
#include <filament/Viewport.h> // Added to provide full definition of filament::Viewport
#include <math/mat4.h>

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
    FilamentAsset* asset = (FilamentAsset*) nativeAsset;
    if (!asset) return nullptr;
    View* view = (View*) nativeView;
    if (!view) return nullptr;
    Camera* cam = &view->getCamera();
    PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg || !cam) return nullptr;

    // Update transforms before picking.
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

    // Screen to NDC conversion. Viewport origin is lower-left in Filament; Java gives top-left.
    filament::Viewport vp = view->getViewport();
    if (vp.width <= 0 || vp.height <= 0) return nullptr;

    // Flip y: incoming sy has top-left origin.
    int flippedY = (int)vp.height - 1 - sy;
    double nx = (double(sx) / double(vp.width)) * 2.0 - 1.0;
    double ny = (double(flippedY) / double(vp.height)) * 2.0 - 1.0;

    // Build ray.
    using namespace filament::math;
    mat4 proj = cam->getProjectionMatrix();
    bool isPerspective = std::abs(proj[3][3]) < 1e-6;
    mat4 invProj = Camera::inverseProjection(proj);
    mat4 viewM = cam->getViewMatrix();
    mat4 invView = inverse(viewM);

    float3 rayOrigin;
    float3 rayDir;
    if (isPerspective) {
        double4 clip{ nx, ny, -1.0, 1.0 }; // near plane
        double4 viewSpace = invProj * clip;
        float3 viewPoint = float3(viewSpace.x, viewSpace.y, viewSpace.z);
        float3 dirView = viewPoint;
        float3 dirWorld = (invView * float4(dirView, 0)).xyz;
        rayOrigin = cam->getPosition();
        rayDir = dirWorld;
    } else {
        double4 clip{ nx, ny, -1.0, 1.0 };
        double4 viewSpace = invProj * clip;
        float3 viewPoint = float3(viewSpace.x, viewSpace.y, viewSpace.z);
        float3 worldPoint = (invView * float4(viewPoint, 1)).xyz;
        rayOrigin = worldPoint;
        rayDir = cam->getForwardVector();
    }

    auto hit = reg->pick(rayOrigin, rayDir);
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
