#include "valdi_core/cpp/Utils/BigInt.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/jni/JavaClass.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

extern "C" JNIEXPORT jlong JNICALL Java_com_snap_valdi_BigInt_nativeParse(JNIEnv* env,
                                                                          jclass /*cls*/,
                                                                          jstring decimal) {
    Valdi::SimpleExceptionTracker tracker;
    auto value = Valdi::BigInt::make(ValdiAndroid::toStdString(ValdiAndroid::JavaEnv(), decimal), tracker);
    if (!tracker) {
        tracker.clearError();
        auto exceptionClass =
            ValdiAndroid::JavaClass::resolveOrAbort(ValdiAndroid::JavaEnv(), "java/lang/IllegalArgumentException");
        env->ThrowNew(exceptionClass.getClass(), "Invalid BigInt decimal string");
        return 0;
    }
    return ValdiAndroid::bridgeRetain(value.get());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_snap_valdi_BigInt_nativeToString(JNIEnv* /*env*/,
                                                                               jclass /*cls*/,
                                                                               jlong handle) {
    auto* value = Valdi::unsafeBridgeUnretained<Valdi::BigInt>(reinterpret_cast<void*>(handle));
    return ValdiAndroid::newJavaStringUTF8(value->toString()).release();
}

extern "C" JNIEXPORT void JNICALL Java_com_snap_valdi_BigInt_nativeRelease(JNIEnv* /*env*/,
                                                                           jclass /*cls*/,
                                                                           jlong handle) {
    ValdiAndroid::bridgeRelease(handle);
}
