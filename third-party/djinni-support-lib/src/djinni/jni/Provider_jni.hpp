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

#pragma once

#include "djinni_support.hpp"
#include "Marshal.hpp"
#include <functional>
#include <memory>

namespace djinni {

struct ProviderJniInfo {
    const GlobalRef<jclass> clazz { jniFindClass("javax/inject/Provider") };
    const jmethodID method_get { jniGetMethodID(clazz.get(), "get", "()Ljava/lang/Object;") };
};

// Forward declaration of type-erased holder
struct ProviderFunctionHolder {
    virtual ~ProviderFunctionHolder() = default;
    virtual jobject call(JNIEnv* env) = 0;
};

// Template implementation for specific types
template<typename T>
struct TypedProviderFunctionHolder : ProviderFunctionHolder {
    using CppType = typename T::CppType;
    using CppProviderType = std::function<CppType()>;
    
    CppProviderType func;
    
    TypedProviderFunctionHolder(CppProviderType f) : func(std::move(f)) {}
    
    jobject call(JNIEnv* env) override {
        auto result = func();
        auto jresult = T::Boxed::fromCpp(env, result);
        return jresult.release();
    }
};

struct NativeProviderHandlerJniInfo {
    const GlobalRef<jclass> clazz { jniFindClass("com/snapchat/djinni/NativeProviderHandler") };
    const jmethodID constructor { jniGetMethodID(clazz.get(), "<init>", "(J)V") };
};

template <class T>
class ProviderAdaptor {
    using CppType = std::function<typename T::CppType()>;
    using CppResType = typename T::CppType;
    using JniResType = typename T::Boxed::JniType;

public:
    using JniType = jobject;
    using Boxed = ProviderAdaptor;

    static CppType toCpp(JNIEnv* jniEnv, JniType j) {
        // Capture the Java Provider object in a GlobalRef wrapped in shared_ptr
        auto provider = std::make_shared<GlobalRef<jobject>>(jniEnv, j);
        
        return [provider]() -> CppResType {
            JNIEnv* jniEnv = jniGetThreadEnv();
            const auto& providerJniInfo = JniClass<ProviderJniInfo>::get();
            auto result = LocalRef<jobject>(jniEnv, jniEnv->CallObjectMethod(
                provider->get(), 
                providerJniInfo.method_get
            ));
            jniExceptionCheck(jniEnv);
            return T::Boxed::toCpp(jniEnv, static_cast<JniResType>(result.get()));
        };
    }

    static LocalRef<JniType> fromCpp(JNIEnv* jniEnv, const CppType& c) {
        // Use unique_ptr so holder is freed if NewObject or jniExceptionCheck throws
        auto holder = std::make_unique<TypedProviderFunctionHolder<T>>(c);

        const auto& nativeProviderHandlerJniInfo = JniClass<NativeProviderHandlerJniInfo>::get();

        auto handler = LocalRef<jobject>(jniEnv, jniEnv->NewObject(
            nativeProviderHandlerJniInfo.clazz.get(),
            nativeProviderHandlerJniInfo.constructor,
            reinterpret_cast<jlong>(holder.get())
        ));
        jniExceptionCheck(jniEnv);

        (void) holder.release();  // Java object now owns the native holder
        return handler;
    }
};

} // namespace djinni
