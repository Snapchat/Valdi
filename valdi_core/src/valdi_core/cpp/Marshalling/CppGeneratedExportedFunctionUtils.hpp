#pragma once

#include "valdi_core/cpp/Marshalling/CppGeneratedClass.hpp"
#include "valdi_core/cpp/Marshalling/CppGeneratedExportedFunction.hpp"
#include "valdi_core/cpp/Marshalling/CppMarshaller.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"

namespace snap::valdi_core {
class JSRuntime;
class JSRuntimeNativeObjectsManager;
} // namespace snap::valdi_core

namespace Valdi {

class RegisteredCppGeneratedClass;

class CppGeneratedExportedFunctionUtils {
public:
    static RegisteredCppGeneratedClass* registerFunctionSchema(const char* schemaString);
    static RegisteredCppGeneratedClass* registerFunctionSchema(const char* schemaString,
                                                               GetTypeReferencesCallback getTypeReferencesFunction);

    /// Resolve an @ExportFunction module as a ValueTypedObject without C++ type instantiation.
    /// Unlike resolve<T>(), this does not require the C++ callable type to be instantiable,
    /// making it suitable for bridging scenarios where only the typed schema is needed.
    static Result<Ref<ValueTypedObject>> resolveAsTypedObject(
        snap::valdi_core::JSRuntime& jsRuntime,
        const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
        const char* modulePath,
        RegisteredCppGeneratedClass& registeredClass);

    template<typename T>
    static Result<T> resolve(
        snap::valdi_core::JSRuntime& jsRuntime,
        const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
        const char* modulePath,
        RegisteredCppGeneratedClass& registeredClass) {
        SimpleExceptionTracker exceptionTracker;
        auto value = resolveModule(exceptionTracker, jsRuntime, nativeObjectsManager, modulePath, registeredClass);
        if (!exceptionTracker) {
            return exceptionTracker.extractError();
        }

        typename T::Callable callable;
        CppMarshaller::unmarshall(exceptionTracker, value, callable);

        if (!exceptionTracker) {
            return exceptionTracker.extractError();
        }

        return T(std::move(callable));
    }

private:
    static Value resolveModule(
        ExceptionTracker& exceptionTracker,
        snap::valdi_core::JSRuntime& jsRuntime,
        const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
        const char* modulePath,
        RegisteredCppGeneratedClass& registeredClass);
};

} // namespace Valdi