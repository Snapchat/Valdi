//
//  CppGeneratedClass.cpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 4/11/23.
//

#include "valdi_core/cpp/Marshalling/CppGeneratedClass.hpp"
#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaTypeResolver.hpp"
#include "valdi_core/cpp/Utils/PlatformObjectAttachments.hpp"

namespace Valdi {

CppGeneratedClass::CppGeneratedClass(RegisteredCppGeneratedClass* /*registeredClass*/) {}

CppGeneratedClass::~CppGeneratedClass() = default;

RegisteredCppGeneratedClass* CppGeneratedClass::registerSchema(const char* schemaString,
                                                               GetTypeReferencesCallback getTypeReferencesCallback) {
    return new RegisteredCppGeneratedClass(
        ValueSchemaRegistry::sharedInstance().get(), schemaString, std::move(getTypeReferencesCallback), {});
}

RegisteredCppGeneratedClass* CppGeneratedClass::registerSchema(const char* schemaString) {
    return registerSchema(schemaString, []() -> TypeReferencesVec { return {}; });
}

RegisteredCppGeneratedClass* CppGeneratedClass::registerGenericSchema(
    const char* schemaString,
    GetTypeReferencesCallback getTypeReferencesCallback,
    GetTypeArgumentsCallback getTypeArgumentsCallback) {
    return new RegisteredCppGeneratedClass(ValueSchemaRegistry::sharedInstance().get(),
                                           schemaString,
                                           std::move(getTypeReferencesCallback),
                                           std::move(getTypeArgumentsCallback));
}

RegisteredCppGeneratedClass* CppGeneratedClass::registerGenericSchema(
    const char* schemaString, GetTypeArgumentsCallback getTypeArgumentsCallback) {
    return registerGenericSchema(schemaString, []() -> TypeReferencesVec { return {}; }, getTypeArgumentsCallback);
}

CppGeneratedModel::CppGeneratedModel(RegisteredCppGeneratedClass* registeredClass)
    : CppGeneratedClass(registeredClass) {}

CppGeneratedModel::CppGeneratedModel(const CppGeneratedModel& other)
    : CppGeneratedClass(/* fine right now because field is unused */ nullptr) {}

CppGeneratedModel::CppGeneratedModel(CppGeneratedModel&& other) noexcept
    : CppGeneratedClass(/* fine right now because field is unused */ nullptr) {}

CppGeneratedModel& CppGeneratedModel::operator=(const CppGeneratedModel& other) {
    return *this;
}
CppGeneratedModel& CppGeneratedModel::operator=(CppGeneratedModel&& other) noexcept {
    return *this;
}

CppGeneratedInterface::CppGeneratedInterface(RegisteredCppGeneratedClass* registeredClass)
    : CppGeneratedClass(registeredClass), _objectAttachments(makeShared<PlatformObjectAttachments>()) {}
CppGeneratedInterface::~CppGeneratedInterface() = default;

const Ref<PlatformObjectAttachments>& CppGeneratedInterface::getObjectAttachments() const {
    return _objectAttachments;
}

} // namespace Valdi
