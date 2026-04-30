#include "valdi_core/jni/JNIMethodUtils.hpp"
#include "valdi_core/jni/JavaUtils.hpp"
#include <fbjni/fbjni.h>

#include "valdi_core/cpp/Utils/ValueFunction.hpp"

namespace fbjni = facebook::jni;

namespace ValdiAndroid {

class ValdiFunctionNative : public fbjni::JavaClass<ValdiFunctionNative> {
public:
    static constexpr auto kJavaDescriptor = "Lcom/snap/valdi/callable/ValdiFunctionNative;";

    //  NOLINTNEXTLINE
    static jboolean nativePerform(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                  jlong ptr,
                                  jint flags,
                                  jlong marshallerPtr) {
        auto* env = fbjni::Environment::current();
        auto* value = reinterpret_cast<Valdi::Value*>(ptr);
        if (value == nullptr) {
            ValdiAndroid::throwJavaValdiException(env, "Cannot call native function after it has been destroyed");
            return static_cast<jboolean>(false);
        }

        const auto& function = value->getFunction();
        auto* marshaller = ValdiAndroid::unwrapMarshaller(env, marshallerPtr);
        if (marshaller == nullptr) {
            return static_cast<jboolean>(false);
        }
        auto retValue = (*function)(Valdi::ValueFunctionCallContext(static_cast<Valdi::ValueFunctionFlags>(flags),
                                                                    marshaller->getValues(),
                                                                    marshaller->size(),
                                                                    marshaller->getExceptionTracker()));
        if (!marshaller->getExceptionTracker()) {
            ValdiAndroid::throwJavaValdiException(env, marshaller->getExceptionTracker().extractError());
            return static_cast<jboolean>(false);
        }

        marshaller->push(std::move(retValue));
        return static_cast<jboolean>(true);
    }

    //  NOLINTNEXTLINE
    static jboolean nativePerformWithTimeout(
        fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint flags, jlong marshallerPtr, jlong timeoutMs) {
        auto* env = fbjni::Environment::current();
        auto* value = reinterpret_cast<Valdi::Value*>(ptr);
        if (value == nullptr) {
            ValdiAndroid::throwJavaValdiException(env, "Cannot call native function after it has been destroyed");
            return static_cast<jboolean>(false);
        }

        const auto& function = value->getFunction();
        auto* marshaller = ValdiAndroid::unwrapMarshaller(env, marshallerPtr);
        if (marshaller == nullptr) {
            return static_cast<jboolean>(false);
        }

        // Note: callSyncWithDeadline creates its own SimpleExceptionTracker and ignores the flags parameter.
        // This means:
        // 1. The FLAGS_PROPAGATES_ERROR flag (if set) is not honored - errors will not be propagated as exceptions
        // 2. We cannot check marshaller->getExceptionTracker() because it's not used by callSyncWithDeadline
        //
        // This limitation is acceptable for the current use case (hit test callbacks) which don't require
        // error propagation and just need a timeout to prevent ANR.
        //
        // TODO: Enhance callSyncWithDeadline to accept ValueFunctionCallContext to support full flag/exception
        // handling.
        auto timeout = std::chrono::milliseconds(timeoutMs);
        auto result = function->callSyncWithDeadline(
            timeout, const_cast<Valdi::Value*>(marshaller->getValues()), marshaller->size());

        if (!result) {
            // Timeout or error occurred. Return false (default/safe value for hit test callbacks).
            return static_cast<jboolean>(false);
        }

        // Push the successful result to the marshaller
        marshaller->push(std::move(result.moveValue()));
        return static_cast<jboolean>(true);
    }

    static void registerNatives() {
        javaClassStatic()->registerNatives({
            makeNativeMethod("nativePerform", ValdiFunctionNative::nativePerform),
            makeNativeMethod("nativePerformWithTimeout", ValdiFunctionNative::nativePerformWithTimeout),
        });
    }
};

} // namespace ValdiAndroid
