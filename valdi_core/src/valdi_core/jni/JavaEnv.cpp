//
//  JavaEnv.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/13/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/jni/JavaEnv.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/jni/JNIConstants.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaClass.hpp"
#include "valdi_core/jni/JavaException.hpp"
#include "valdi_core/jni/JavaMethod.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

namespace ValdiAndroid {

JavaEnv::JavaEnv() = default;

JavaCache& JavaEnv::getCache() {
    return JavaCache::get();
}

thread_local JNIExceptionHandler* tExceptionHandler = nullptr;

void JavaEnv::setExceptionHandlerForCurrentThread(JNIExceptionHandler* exceptionHandler) {
    tExceptionHandler = exceptionHandler;
}

JNIExceptionHandler* JavaEnv::getExceptionHandlerForCurrentThread() {
    return tExceptionHandler;
}

bool JavaEnv::checkException() {
    auto* env = getUnsafeEnv();

    if (VALDI_UNLIKELY(env->ExceptionCheck() != 0u)) {
        auto exception = JavaException(JavaEnv(), djinni::LocalRef<jobject>(env->ExceptionOccurred()));
        env->ExceptionClear();

        auto* exceptionHandler = JavaEnv::getExceptionHandlerForCurrentThread();
        if (exceptionHandler != nullptr) {
            exceptionHandler->onException(std::move(exception));
        } else {
            exception.handleFatal(env, std::nullopt);
        }

        return false;
    }

    return true;
}

JNIEnv* JavaEnv::getUnsafeEnv() {
    thread_local JNIEnv* attachedEnv = nullptr;

    auto* env = attachedEnv;
    if (env != nullptr) {
        return env;
    }

    env = djinni::jniGetThreadEnv();
    attachedEnv = env;
    return env;
}

bool JavaEnv::instanceOf(jobject object, const ValdiAndroid::JavaClass& cls) {
    return static_cast<bool>(getUnsafeEnv()->IsInstanceOf(object, cls.getClass()));
}

::djinni::GlobalRef<jobject> JavaEnv::newGlobalRef(jobject obj) {
    return ::djinni::GlobalRef<jobject>(getUnsafeEnv(), obj);
}

::djinni::GlobalRef<jclass> JavaEnv::newGlobalRef(jclass cls) {
    return ::djinni::GlobalRef<jclass>(getUnsafeEnv(), cls);
}

::djinni::LocalRef<jobject> JavaEnv::newLocalRef(jobject obj) {
    if (obj == nullptr) {
        return ::djinni::LocalRef<jobject>();
    }
    auto* ref = getUnsafeEnv()->NewLocalRef(obj);
    return ::djinni::LocalRef<jobject>(ref);
}

bool JavaEnv::isSameObject(jobject object, jobject otherObject) {
    if (object == nullptr && otherObject == nullptr) {
        return true;
    }
    if ((object == nullptr) != (otherObject == nullptr)) {
        return false;
    }
    return static_cast<bool>(getUnsafeEnv()->IsSameObject(object, otherObject));
}

} // namespace ValdiAndroid
