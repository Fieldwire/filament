/*
 * Copyright (C) 2026 The Android Open Source Project
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

#include <gltfio/MaterialProvider.h>

#include <utils/debug.h>
#include <utils/FixedCapacityVector.h>

#include <vector>
#include <string>
#include <algorithm>

#include "MaterialKey.h"

using namespace filament;
using namespace filament::gltfio;

static utils::FixedCapacityVector<char const*> convertVariantFilters(JNIEnv* env, jobjectArray filters) {
    utils::FixedCapacityVector<char const*> result;
    if (!filters) return result;
    const jsize len = env->GetArrayLength(filters);
    result.reserve(len);
    for (jsize i = 0; i < len; i++) {
        auto jstr = (jstring) env->GetObjectArrayElement(filters, i);
        if (!jstr) continue;
        const char* cstr = env->GetStringUTFChars(jstr, nullptr);
        result.push_back(cstr);
        env->ReleaseStringUTFChars(jstr, cstr);
        env->DeleteLocalRef(jstr);
    }
    return result;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_google_android_filament_gltfio_JitShaderProvider_nCreateJitShaderProvider(JNIEnv* env, jclass,
        jlong nativeEngine, jboolean optimize, jobjectArray variantFilters) {
    Engine* engine = (Engine*) nativeEngine;

    utils::FixedCapacityVector<char const*> filterPtrs;
    std::vector<std::string> keepAlive;
    if (variantFilters) {
        const jsize len = env->GetArrayLength(variantFilters);
        keepAlive.reserve((size_t)len);
        filterPtrs.reserve((size_t)len);
        for (jsize i = 0; i < len; i++) {
            auto jstr = (jstring) env->GetObjectArrayElement(variantFilters, i);
            if (!jstr) continue;
            const char* cstr = env->GetStringUTFChars(jstr, nullptr);
            if (cstr) {
                keepAlive.emplace_back(cstr);
                env->ReleaseStringUTFChars(jstr, cstr);
            }
            env->DeleteLocalRef(jstr);
        }
        for (auto& s : keepAlive) {
            filterPtrs.push_back(s.c_str());
        }
    }

    return (jlong) createJitShaderProvider(engine, (bool)optimize, filterPtrs);
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_JitShaderProvider_nDestroyJitShaderProvider(JNIEnv*, jclass,
        jlong nativeProvider) {
    auto provider = (MaterialProvider*) nativeProvider;
    delete provider;
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_JitShaderProvider_nDestroyMaterials(JNIEnv*, jclass,
        jlong nativeProvider) {
    auto provider = (MaterialProvider*) nativeProvider;
    provider->destroyMaterials();
}

extern "C" JNIEXPORT long JNICALL
Java_com_google_android_filament_gltfio_JitShaderProvider_nCreateMaterialInstance(JNIEnv* env, jclass,
        jlong nativeProvider, jobject materialKey, jintArray uvmap, jstring label, jstring extras) {
    MaterialKey nativeKey = {};

    auto& helper = MaterialKeyHelper::get();
    helper.copy(env, nativeKey, materialKey);

    const char* nativeLabel = label ? env->GetStringUTFChars(label, nullptr) : nullptr;
    const char* nativeExtras = extras ? env->GetStringUTFChars(extras, nullptr) : nullptr;

    UvMap nativeUvMap = {};
    auto provider = (MaterialProvider*) nativeProvider;
    MaterialInstance* instance = provider->createMaterialInstance(&nativeKey, &nativeUvMap,
            nativeLabel, nativeExtras);

    // Copy the UvMap results from the native array into the JVM array.
    jint* elements = env->GetIntArrayElements(uvmap, nullptr);
    if (elements) {
        const size_t javaSize = env->GetArrayLength(uvmap);
        for (int i = 0, n = std::min(javaSize, nativeUvMap.size()); i < n; ++i) {
            elements[i] = nativeUvMap[i];
        }
        env->ReleaseIntArrayElements(uvmap, elements, 0);
    }

    // The config parameter is an in-out parameter so we need to copy the results back to Java.
    helper.copy(env, materialKey, nativeKey);

    if (label) {
        env->ReleaseStringUTFChars(label, nativeLabel);
    }

    if (extras) {
        env->ReleaseStringUTFChars(extras, nativeExtras);
    }

    return (long) instance;
}

extern "C" JNIEXPORT long JNICALL
Java_com_google_android_filament_gltfio_JitShaderProvider_nGetMaterial(JNIEnv* env, jclass,
        jlong nativeProvider, jobject materialKey, jintArray uvmap, jstring label) {
    MaterialKey nativeKey = {};

    auto& helper = MaterialKeyHelper::get();
    helper.copy(env, nativeKey, materialKey);

    const char* nativeLabel = label ? env->GetStringUTFChars(label, nullptr) : nullptr;

    UvMap nativeUvMap = {};
    auto provider = (MaterialProvider*) nativeProvider;
    Material* material = provider->getMaterial(&nativeKey, &nativeUvMap, nativeLabel);

    // Copy the UvMap results from the native array into the JVM array.
    jint* elements = env->GetIntArrayElements(uvmap, nullptr);
    if (elements) {
        const size_t javaSize = env->GetArrayLength(uvmap);
        for (int i = 0, n = std::min(javaSize, nativeUvMap.size()); i < n; ++i) {
            elements[i] = nativeUvMap[i];
        }
        env->ReleaseIntArrayElements(uvmap, elements, 0);
    }

    // The config parameter is an in-out parameter so we need to copy the results back to Java.
    helper.copy(env, materialKey, nativeKey);

    if (label) {
        env->ReleaseStringUTFChars(label, nativeLabel);
    }

    return (long) material;
}

extern "C" JNIEXPORT int JNICALL
Java_com_google_android_filament_gltfio_JitShaderProvider_nGetMaterialCount(JNIEnv*, jclass,
        jlong nativeProvider) {
    auto provider = (MaterialProvider*) nativeProvider;
    return provider->getMaterialsCount();
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_gltfio_JitShaderProvider_nGetMaterials(JNIEnv* env, jclass,
        jlong nativeProvider, jlongArray result) {
    auto provider = (MaterialProvider *) nativeProvider;
    auto materials = provider->getMaterials();
    jlong *resultElements = env->GetLongArrayElements(result, nullptr);
    if (resultElements) {
        const size_t javaSize = env->GetArrayLength(result);
        for (int i = 0, n = std::min(javaSize, provider->getMaterialsCount()); i < n; ++i) {
            resultElements[i] = (jlong) materials[i];
        }
        env->ReleaseLongArrayElements(result, resultElements, JNI_ABORT);
    }
}

