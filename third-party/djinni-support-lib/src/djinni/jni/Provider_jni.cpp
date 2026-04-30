/**
 * Copyright 2021 Snap, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "djinni_support.hpp"
#include "Provider_jni.hpp"

// Implementation of JNI native methods for NativeProviderHandler
// The type-erased holders are defined in Provider_jni.hpp

extern "C" {

CJNIEXPORT jobject JNICALL
Java_com_snapchat_djinni_NativeProviderHandler_nativeGet(JNIEnv* env, jclass, jlong nativeRef) {
    try {
        auto* holder = reinterpret_cast<djinni::ProviderFunctionHolder*>(nativeRef);
        return holder->call(env);
    } JNI_TRANSLATE_EXCEPTIONS_RETURN(env, nullptr)
}

CJNIEXPORT void JNICALL
Java_com_snapchat_djinni_NativeProviderHandler_nativeDestroy(JNIEnv* env, jclass, jlong nativeRef) {
    try {
        auto* holder = reinterpret_cast<djinni::ProviderFunctionHolder*>(nativeRef);
        delete holder;
    } JNI_TRANSLATE_EXCEPTIONS_RETURN(env, )
}

} // extern "C"
