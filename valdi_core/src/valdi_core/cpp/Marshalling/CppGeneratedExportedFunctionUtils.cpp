#include "valdi_core/cpp/Marshalling/CppGeneratedExportedFunctionUtils.hpp"
#include "valdi_core/JSRuntime.hpp"
#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"

namespace Valdi {

RegisteredCppGeneratedClass* CppGeneratedExportedFunctionUtils::registerFunctionSchema(const char* schemaString) {
    return registerFunctionSchema(schemaString, []() -> TypeReferencesVec { return {}; });
}

RegisteredCppGeneratedClass* CppGeneratedExportedFunctionUtils::registerFunctionSchema(
    const char* schemaString, GetTypeReferencesCallback getTypeReferencesFunction) {
    return new RegisteredCppGeneratedClass(
        ValueSchemaRegistry::sharedInstance().get(), schemaString, std::move(getTypeReferencesFunction), {});
}

Result<Ref<ValueTypedObject>> CppGeneratedExportedFunctionUtils::resolveAsTypedObject(
    snap::valdi_core::JSRuntime& jsRuntime,
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    const char* modulePath,
    RegisteredCppGeneratedClass& registeredClass) {
    SimpleExceptionTracker exceptionTracker;
    auto schema = registeredClass.getResolvedSchema(exceptionTracker);
    if (!exceptionTracker) {
        return exceptionTracker.extractError();
    }

    Marshaller marshaller(exceptionTracker);
    marshaller.setCurrentSchema(schema);

    auto objectIndex = jsRuntime.pushModuleToMarshaller(
        nativeObjectsManager, Valdi::StringBox::fromCString(modulePath), reinterpret_cast<int64_t>(&marshaller));
    if (!exceptionTracker) {
        return exceptionTracker.extractError();
    }

    auto typedObject = marshaller.getTypedObject(objectIndex);
    if (!exceptionTracker) {
        return exceptionTracker.extractError();
    }

    return typedObject;
}

Value CppGeneratedExportedFunctionUtils::resolveModule(
    ExceptionTracker& exceptionTracker,
    snap::valdi_core::JSRuntime& jsRuntime,
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    const char* modulePath,
    RegisteredCppGeneratedClass& registeredClass) {
    auto result = resolveAsTypedObject(jsRuntime, nativeObjectsManager, modulePath, registeredClass);
    if (!result) {
        exceptionTracker.onError(result.moveError());
        return Value();
    }

    return result.value()->getProperty(0);
}

} // namespace Valdi