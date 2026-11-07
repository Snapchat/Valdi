#include "valdi_core/jni/JavaException.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

namespace ValdiAndroid {

static std::string handleFatalError(JNIEnv* env, jthrowable throwable, std::optional<std::string> additionalInfo) {
    const auto* defaultErrorMessage = "Unknown Java Exception occurred";

    auto* exceptionHandlerClass = env->FindClass("com/snap/valdi/exceptions/GlobalExceptionHandler");
    if (env->ExceptionCheck() != 0u) {
        env->ExceptionClear();
        return defaultErrorMessage;
    }

    auto methodSignature = getMethodSignature<String, Throwable, String>();
    auto* methodId = env->GetStaticMethodID(exceptionHandlerClass, "onFatalExceptionFromCPP", methodSignature.c_str());
    if (env->ExceptionCheck() != 0u) {
        env->ExceptionClear();
        return defaultErrorMessage;
    }

    jstring additionalInfoString = nullptr;
    if (additionalInfo) {
        additionalInfoString = env->NewStringUTF(additionalInfo.value().c_str());

        if (env->ExceptionCheck() != 0u) {
            env->ExceptionClear();
            return defaultErrorMessage;
        }
    }

    auto* errorMessageJString = reinterpret_cast<jstring>(
        env->CallStaticObjectMethod(exceptionHandlerClass, methodId, throwable, additionalInfoString));
    if (env->ExceptionCheck() != 0u) {
        env->ExceptionClear();
        return defaultErrorMessage;
    }

    auto length = env->GetStringUTFLength(errorMessageJString);
    if (env->ExceptionCheck() != 0u) {
        env->ExceptionClear();
        return defaultErrorMessage;
    }

    const auto* errorMessageUTF = env->GetStringUTFChars(errorMessageJString, nullptr);
    if (env->ExceptionCheck() != 0u) {
        env->ExceptionClear();
        return defaultErrorMessage;
    }

    return std::string(errorMessageUTF, static_cast<size_t>(length));
}

JavaException::JavaException(JavaEnv vm, djinni::LocalRef<jobject> exception) : JavaObject(vm, std::move(exception)) {}
JavaException::~JavaException() = default;

void JavaException::handleFatal(JNIEnv* env, std::optional<std::string> additionalInfo) const {
    auto* throwable = reinterpret_cast<jthrowable>(getUnsafeObject());
    auto errorMessage = handleFatalError(env, throwable, std::move(additionalInfo));
    env->FatalError(errorMessage.data());
}

std::string JavaException::getMessage() const {
    auto message = JavaCache::get().getThrowableGetMessageMethod().call(getUnsafeObject());
    if (message.isNull()) {
        return "";
    }

    return toStdString(JavaEnv(), message);
}

} // namespace ValdiAndroid
