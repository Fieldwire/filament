/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "private/backend/VirtualMachineEnv.h"

#include <utils/compiler.h>
#include <utils/debug.h>

#include <jni.h>

namespace filament {

JavaVM* VirtualMachineEnv::sVirtualMachine = nullptr;

/*
 * This is typically called by filament_jni.so when it is loaded. If filament_jni.so is not used,
 * then this must be called manually -- however, this is a problem because VirtualMachineEnv.h
 * is currently private and part of backend.
 * For now, we authorize this usage, but we will need to fix it; by making a proper public
 * API for this.
 */
UTILS_PUBLIC
UTILS_NOINLINE
jint VirtualMachineEnv::JNI_OnLoad(JavaVM* vm) noexcept {
    JNIEnv* env = nullptr;
    if (UTILS_UNLIKELY(vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)) {
        // this should not happen
        return -1;
    }
    sVirtualMachine = vm;
    return JNI_VERSION_1_6;
}

UTILS_NOINLINE
void VirtualMachineEnv::handleException(JNIEnv* const env) noexcept {
    if (UTILS_UNLIKELY(env->ExceptionCheck())) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

UTILS_NOINLINE
JNIEnv* VirtualMachineEnv::getEnvironmentSlow() noexcept {
#if defined(__ANDROID__)
    mVirtualMachine->AttachCurrentThread(&mJniEnv, nullptr);
#else
    mVirtualMachine->AttachCurrentThread(reinterpret_cast<void**>(&mJniEnv), nullptr);
#endif
    assert_invariant(mJniEnv);
    return mJniEnv;
}

} // namespace filament

