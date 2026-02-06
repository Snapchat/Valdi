#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaParser.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaTypeResolver.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/PlatformObjectAttachments.hpp"
#include "valdi_core/cpp/Utils/TextParser.hpp"
#include <fmt/format.h>

namespace Valdi {

RegisteredCppGeneratedClass::RegisteredCppGeneratedClass(ValueSchemaRegistry* registry,
                                                         const char* schemaString,
                                                         GetTypeReferencesFunction getTypeReferencesFunction,
                                                         GetTypeArgumentsFunction getTypeArgumentsFunction)
    : _registry(registry),
      _schemaString(schemaString),
      _getTypeReferencesFunction(std::move(getTypeReferencesFunction)) {
    if (getTypeArgumentsFunction) {
        _genericData = std::make_unique<GenericData>();
        _genericData->getTypeArguments = std::move(getTypeArgumentsFunction);
    }
}

RegisteredCppGeneratedClass::RegisteredCppGeneratedClass(ValueSchemaRegistry* registry,
                                                         const char* schemaString,
                                                         bool isEnum)
    : _registry(registry), _schemaString(schemaString), _isEnum(isEnum) {}

RegisteredCppGeneratedClass::~RegisteredCppGeneratedClass() = default;

std::string RegisteredCppGeneratedClass::resolveSchemaString() const {
    auto resolvedSchemaString = std::string(_schemaString);
    if (_getTypeReferencesFunction) {
        auto dependencies = _getTypeReferencesFunction();
        size_t index = 0;
        for (const auto& dependency : dependencies) {
            auto pattern = fmt::format("[{}]", index);
            stringReplace(resolvedSchemaString, pattern, dependency->getClassName().toStringView());
            index++;
        }
    }

    return resolvedSchemaString;
}

void RegisteredCppGeneratedClass::ensureSchemaRegistered(ExceptionTracker& exceptionTracker) {
    if (!_schemaRegistered) {
        auto lock = _registry->lock();
        if (_schemaRegistered) {
            return;
        }

        auto resolvedSchemaString = resolveSchemaString();

        auto parseResult = ValueSchema::parse(resolvedSchemaString);
        if (!parseResult) {
            exceptionTracker.onError(parseResult.moveError());
            return;
        }

        _schemaIdentifier = _registry->registerSchema(parseResult.value());
        if (_isEnum) {
            _resolvedSchema = parseResult.value();
            _schemaResolved = true;
            return;
        }

        _schemaRegistered = true;

        if (_getTypeReferencesFunction) {
            auto dependencies = _getTypeReferencesFunction();
            for (const auto& dependency : dependencies) {
                dependency->ensureSchemaRegistered(exceptionTracker);
                if (!exceptionTracker) {
                    return;
                }
            }
        }
    }
}

Result<Ref<ClassSchema>> RegisteredCppGeneratedClass::getResolvedClassSchema() {
    SimpleExceptionTracker exceptionTracker;
    auto classSchema = getResolvedClassSchema(exceptionTracker);
    if (!exceptionTracker) {
        return exceptionTracker.extractError();
    }
    return classSchema;
}

Ref<ClassSchema> RegisteredCppGeneratedClass::getResolvedClassSchema(ExceptionTracker& exceptionTracker) {
    auto schema = getResolvedSchema(exceptionTracker);
    return schema.getClassRef();
}

ValueSchema RegisteredCppGeneratedClass::getResolvedSchema(ExceptionTracker& exceptionTracker) {
    ensureSchemaRegistered(exceptionTracker);
    if (!exceptionTracker) {
        return ValueSchema::voidType();
    }

    if (!_schemaResolved) {
        auto lock = _registry->lock();
        if (_schemaResolved) {
            return _resolvedSchema;
        }

        std::vector<RegisteredCppGeneratedClass*> dependencies;
        if (_getTypeReferencesFunction) {
            dependencies = _getTypeReferencesFunction();
        }

        ValueSchemaTypeResolver typeResolver(_registry);

        auto schema = _registry->getSchemaForIdentifier(_schemaIdentifier);
        bool didChange = false;

        std::vector<ValueSchema> typeArguments;
        if (_genericData) {
            typeArguments = _genericData->getTypeArguments(exceptionTracker);
            if (!exceptionTracker) {
                return ValueSchema::voidType();
            }
        }

        auto result = typeResolver.resolveTypeReferences(
            schema, ValueSchemaRegistryResolveType::Schema, typeArguments.data(), typeArguments.size(), &didChange);
        if (!result) {
            exceptionTracker.onError(result.moveError());
            return ValueSchema::voidType();
        }

        if (didChange && _genericData == nullptr) {
            _registry->updateSchema(_schemaIdentifier, result.value());
        }

        _resolvedSchema = result.value();
        _schemaResolved = true;

        // Resolve dependencies. We do it with _schemaResolved set to true
        // so that circular dependencies can be properly managed.
        for (const auto& dependency : dependencies) {
            dependency->getResolvedSchema(exceptionTracker);
            if (!exceptionTracker) {
                _schemaResolved = false;
                return ValueSchema::voidType();
            }
        }
    }

    return _resolvedSchema;
}

const StringBox& RegisteredCppGeneratedClass::getClassName() {
    auto lock = _registry->lock();
    if (_className.isEmpty()) {
        if (_isEnum) {
            SimpleExceptionTracker exceptionTracker;
            auto schema = getResolvedSchema(exceptionTracker);
            if (!exceptionTracker) {
                exceptionTracker.clearError();
            }

            const auto* enumSchema = schema.getEnum();
            if (enumSchema != nullptr) {
                _className = enumSchema->getName();
            }
        } else {
            auto parser = TextParser(std::string_view(_schemaString));
            auto classSchema = ValueSchemaParser::parseClassSchema(parser, false);
            if (classSchema) {
                _className = classSchema.value().getClass()->getClassName();
            }
        }
    }

    return _className;
}

} // namespace Valdi
