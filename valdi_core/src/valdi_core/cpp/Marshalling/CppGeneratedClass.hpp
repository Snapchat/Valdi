//
//  CppGeneratedClass.hpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 4/11/23.
//

#pragma once

#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include <vector>

namespace Valdi {

class ValueSchemaRegistry;
class PlatformObjectAttachments;
class RegisteredCppGeneratedClass;
class ValueSchema;
class ExceptionTracker;

using TypeReferencesVec = std::vector<RegisteredCppGeneratedClass*>;
using ValueSchemaVector = std::vector<ValueSchema>;
using GetTypeReferencesCallback = TypeReferencesVec (*)();
using GetTypeArgumentsCallback = ValueSchemaVector (*)(ExceptionTracker& exceptionTracker);

class CppGeneratedClass {
public:
    explicit CppGeneratedClass(RegisteredCppGeneratedClass* registeredClass);
    ~CppGeneratedClass();

protected:
    static RegisteredCppGeneratedClass* registerSchema(const char* schemaString,
                                                       GetTypeReferencesCallback getTypeReferencesCallback);
    static RegisteredCppGeneratedClass* registerSchema(const char* schemaString);

    static RegisteredCppGeneratedClass* registerGenericSchema(const char* schemaString,
                                                              GetTypeReferencesCallback getTypeReferencesCallback,
                                                              GetTypeArgumentsCallback getTypeArgumentsCallback);

    static RegisteredCppGeneratedClass* registerGenericSchema(const char* schemaString,
                                                              GetTypeArgumentsCallback getTypeArgumentsCallback);
};

class CppGeneratedModel : public SimpleRefCountable, public CppGeneratedClass {
public:
    explicit CppGeneratedModel(RegisteredCppGeneratedClass* registeredClass);

    CppGeneratedModel(const CppGeneratedModel& other);
    CppGeneratedModel(CppGeneratedModel&& other) noexcept;

    CppGeneratedModel& operator=(const CppGeneratedModel& other);
    CppGeneratedModel& operator=(CppGeneratedModel&& other) noexcept;
};

class CppGeneratedInterface : public SharedPtrRefCountable, public CppGeneratedClass {
public:
    explicit CppGeneratedInterface(RegisteredCppGeneratedClass* registeredClass);
    ~CppGeneratedInterface() override;

    const Ref<PlatformObjectAttachments>& getObjectAttachments() const;

private:
    Ref<PlatformObjectAttachments> _objectAttachments;
};

} // namespace Valdi
