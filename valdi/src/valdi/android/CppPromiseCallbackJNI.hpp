#include "valdi_core/cpp/Utils/Promise.hpp"
#include "valdi_core/cpp/Utils/ValueMarshaller.hpp"
#include "valdi_core/jni/JNIMethodUtils.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaUtils.hpp"
#include "valdi_core/jni/JavaValue.hpp"
#include <fbjni/fbjni.h>

namespace fbjni = facebook::jni;

namespace ValdiAndroid {

class CppPromiseCallbackJNI : public fbjni::JavaClass<CppPromiseCallbackJNI> {
public:
    static constexpr auto kJavaDescriptor = "Lcom/snap/valdi/promise/CppPromiseCallback;";

    static Valdi::Ref<Valdi::PromiseCallback> getPromiseCallback(jlong ptr) {
        return bridgeTransfer<Valdi::PromiseCallback>(ptr);
    }

    static Valdi::Ref<Valdi::ValueMarshaller<JavaValue>> getValueMarshaller(jlong ptr) {
        return bridgeTransfer<Valdi::ValueMarshaller<JavaValue>>(ptr);
    }

    // NOLINTNEXTLINE
    static void nativeOnSuccess(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                jlong ptr,
                                jlong valueMarshallerNativeHandle,
                                jobject value) {
        auto promiseCompletion = getPromiseCallback(ptr);
        auto valueMarshaller = getValueMarshaller(valueMarshallerNativeHandle);
        if (promiseCompletion == nullptr || valueMarshaller == nullptr) {
            return;
        }

        Valdi::SimpleExceptionTracker exceptionTracker;
        auto marshalledResult = valueMarshaller->marshall(
            nullptr, JavaValue::unsafeMakeObject(value), Valdi::ReferenceInfoBuilder(), exceptionTracker);
        if (!exceptionTracker) {
            promiseCompletion->onFailure(exceptionTracker.extractError());
        } else {
            promiseCompletion->onSuccess(marshalledResult);
        }
    }

    // NOLINTNEXTLINE
    static void nativeOnFailure(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                jlong ptr,
                                jlong valueMarshallerNativeHandle,
                                jstring error,
                                jint errorCode) {
        auto promiseCompletion = getPromiseCallback(ptr);
        auto valueMarshaller = getValueMarshaller(valueMarshallerNativeHandle);
        if (promiseCompletion == nullptr || valueMarshaller == nullptr) {
            return;
        }

        // Preserve the code the Kotlin side computed (see Throwable.valdiErrorCode) so a
        // cancellation stays distinguishable from a genuine failure past this boundary.
        promiseCompletion->onFailure(Valdi::Error(toInternedString(JavaEnv(), error), errorCode));
    }

    static void registerNatives() {
        javaClassStatic()->registerNatives({
            makeNativeMethod("nativeOnSuccess", CppPromiseCallbackJNI::nativeOnSuccess),
            makeNativeMethod("nativeOnFailure", CppPromiseCallbackJNI::nativeOnFailure),
        });
    }
};

} // namespace ValdiAndroid
