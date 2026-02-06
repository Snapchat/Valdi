#pragma once

#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistrySchemaIdentifier.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include <vector>

namespace Valdi {

class ValueSchemaRegistry;
class ExceptionTracker;
class ClassSchema;
class RegisteredCppGeneratedClass;
class ValueSchema;

using GetTypeReferencesFunction = Function<std::vector<RegisteredCppGeneratedClass*>()>;
using GetTypeArgumentsFunction = Function<std::vector<ValueSchema>(ExceptionTracker& exceptionTracker)>;

class RegisteredCppGeneratedClass {
public:
    RegisteredCppGeneratedClass(ValueSchemaRegistry* registry,
                                const char* schemaString,
                                GetTypeReferencesFunction getTypeReferencesFunction,
                                GetTypeArgumentsFunction getTypeArgumentsFunction);
    RegisteredCppGeneratedClass(ValueSchemaRegistry* registry, const char* schemaString, bool isEnum);
    ~RegisteredCppGeneratedClass();

    void ensureSchemaRegistered(ExceptionTracker& exceptionTracker);

    Ref<ClassSchema> getResolvedClassSchema(ExceptionTracker& exceptionTracker);
    Result<Ref<ClassSchema>> getResolvedClassSchema();
    ValueSchema getResolvedSchema(ExceptionTracker& exceptionTracker);
    const StringBox& getClassName();

private:
    struct GenericData {
        GetTypeArgumentsFunction getTypeArguments;
    };
    ValueSchemaRegistry* _registry;
    const char* _schemaString;
    std::atomic_bool _schemaRegistered = false;
    std::atomic_bool _schemaResolved = false;
    bool _isEnum = false;
    ValueSchemaRegistrySchemaIdentifier _schemaIdentifier = 0;
    StringBox _className;
    ValueSchema _resolvedSchema;
    GetTypeReferencesFunction _getTypeReferencesFunction;
    std::unique_ptr<GenericData> _genericData;

    std::string resolveSchemaString() const;
};

} // namespace Valdi
