//
//  ValueSchemaTypeResolver.cpp
//  valdi_core-ios
//
//  Created by Simon Corsin on 2/2/23.
//

#include "valdi_core/cpp/Schema/ValueSchemaTypeResolver.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace Valdi {

static ValueSchemaRegistryKey makeStartingKey(const ValueSchema* typeArguments, size_t typeArgumentsKey) {
    if (typeArgumentsKey == 0) {
        return ValueSchemaRegistryKey(ValueSchema::voidType());
    } else {
        return ValueSchemaRegistryKey(ValueSchema::genericTypeReference(
            ValueSchemaTypeReference::named(StringBox()), typeArguments, typeArgumentsKey));
    }
}

static Result<ValueSchema> getTypeArgumentFromSchemaKey(const ValueSchemaRegistryKey& currentKey,
                                                        ValueSchemaTypeReference::Position position) {
    auto index = static_cast<size_t>(position);
    const auto* genericTypeReference = currentKey.getSchemaKey().getGenericTypeReference();
    if (genericTypeReference != nullptr && index < genericTypeReference->getTypeArgumentsSize()) {
        return genericTypeReference->getTypeArgument(index);
    }

    return Error(STRING_FORMAT("Missing type argument at position {} (types arguments are {})",
                               position,
                               currentKey.getSchemaKey().toString()));
}

static void syncSchemaResultFlags(Result<ValueSchemaRegistryResolveOutput>& result, const ValueSchema& schema) {
    if (!result) {
        return;
    }

    auto& output = result.value();
    if (schema.isOptional() && !output.schema.isOptional()) {
        output.schema = output.schema.asOptional();
        output.didChange = true;
    }

    if (schema.isBoxed() && !output.schema.isBoxed()) {
        output.schema = ValueSchema::boxed(std::move(output.schema));
        output.didChange = true;
    }
}

ValueSchemaTypeResolver::ValueSchemaTypeResolver() = default;
ValueSchemaTypeResolver::ValueSchemaTypeResolver(ValueSchemaRegistry* registry) : _registry(registry) {}

Result<ValueSchema> ValueSchemaTypeResolver::resolveTypeReferences(const ValueSchema& schema,
                                                                   ValueSchemaRegistryResolveType resolveType,
                                                                   bool* didChange) const {
    return resolveTypeReferences(schema, resolveType, nullptr, 0, didChange);
}

Result<ValueSchema> ValueSchemaTypeResolver::resolveTypeReferences(const ValueSchema& schema,
                                                                   ValueSchemaRegistryResolveType resolveType,
                                                                   const ValueSchema* typeArguments,
                                                                   size_t typeArgumentsLength,
                                                                   bool* didChange) const {
    auto startingKey = makeStartingKey(typeArguments, typeArgumentsLength);

    auto result = resolveTypeReferences(startingKey, resolveType, schema);
    if (VALDI_UNLIKELY(!result)) {
        return result.moveError();
    }

    if (didChange != nullptr) {
        *didChange = result.value().didChange;
    }

    return std::move(result.moveValue().schema);
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferences(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& schema) const {
    auto output = resolveTypeReferencesInner(currentKey, resolveType, schema);

    syncSchemaResultFlags(output, schema);

    return output;
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInner(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& schema) const {
    if (schema.isTypeReference()) {
        return getResolvedSchemaForTypeReference(currentKey, resolveType, schema.getTypeReference(), schema.isBoxed());
    } else if (schema.isGenericTypeReference()) {
        return resolveSchemaForGenericType(
            currentKey, resolveType, *schema.getGenericTypeReference(), schema.isBoxed());
    } else if (schema.isFunction()) {
        return resolveTypeReferencesInFunction(currentKey, resolveType, schema, *schema.getFunction());
    } else if (schema.isClass()) {
        return resolveTypeReferencesInClass(currentKey, resolveType, schema, *schema.getClass());
    } else if (schema.isArray()) {
        return resolveTypeReferencesInArray(currentKey, resolveType, schema, schema.getArrayItemSchema());
    } else if (schema.isMap()) {
        return resolveTypeReferencesInMap(currentKey, resolveType, schema);
    } else if (schema.isES6Map()) {
        return resolveTypeReferencesInES6Map(currentKey, resolveType, schema);
    } else if (schema.isES6Set()) {
        return resolveTypeReferencesInES6Set(currentKey, resolveType, schema);
    } else if (schema.isOutcome()) {
        return resolveTypeReferencesInOutcome(
            currentKey, resolveType, schema, schema.getOutcome()->getValue(), schema.getOutcome()->getError());
    } else if (schema.isPromise()) {
        return resolveTypeReferencesInPromise(currentKey, resolveType, schema, schema.getPromise()->getValueSchema());
    } else if (schema.isSchemaReference() && resolveType == ValueSchemaRegistryResolveType::Key) {
        // Replace Schema references by Schema keys when requested.
        return ValueSchemaRegistryResolveOutput(schema.getSchemaReference()->getKey(), true);
    }

    return ValueSchemaRegistryResolveOutput(schema, false);
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInFunction(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& sourceSchema,
    const ValueFunctionSchema& function) const {
    auto returnResult = resolveTypeReferences(currentKey, resolveType, function.getReturnValue());
    if (VALDI_UNLIKELY(!returnResult)) {
        return returnResult.error().rethrow("Failed to resolve return value");
    }

    auto needCopy = returnResult.value().didChange;

    Valdi::SmallVector<ValueSchema, 10> parameters;
    parameters.reserve(function.getParametersSize());

    for (size_t i = 0; i < function.getParametersSize(); i++) {
        const auto& parameter = function.getParameter(i);
        auto result = resolveTypeReferences(currentKey, resolveType, parameter);
        if (VALDI_UNLIKELY(!result)) {
            return result.error().rethrow(STRING_FORMAT("Failed to resolve parameter at index {}", i));
        }

        if (result.value().didChange) {
            needCopy = true;
        }

        parameters.emplace_back(std::move(result.moveValue().schema));
    }

    if (needCopy) {
        return ValueSchemaRegistryResolveOutput(
            ValueSchema::function(
                function.getAttributes(), returnResult.value().schema, parameters.data(), parameters.size()),
            true);
    } else {
        return ValueSchemaRegistryResolveOutput(sourceSchema, false);
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInClass(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& sourceSchema,
    const ClassSchema& classSchema) const {
    Valdi::SmallVector<ClassPropertySchema, 32> properties;
    properties.reserve(classSchema.getPropertiesSize());
    bool needCopy = false;

    for (size_t i = 0; i < classSchema.getPropertiesSize(); i++) {
        const auto& property = classSchema.getProperty(i);
        auto result = resolveTypeReferences(currentKey, resolveType, property.schema);
        if (VALDI_UNLIKELY(!result)) {
            return result.error().rethrow(
                STRING_FORMAT("Failed to resolve {}.{}", classSchema.getClassName(), property.name));
        }

        if (result.value().didChange) {
            needCopy = true;
        }

        properties.emplace_back(property.name, result.value().schema);
    }

    if (needCopy) {
        return ValueSchemaRegistryResolveOutput(
            ValueSchema::cls(
                classSchema.getClassName(), classSchema.isInterface(), properties.data(), properties.size()),
            true);
    } else {
        return ValueSchemaRegistryResolveOutput(sourceSchema, false);
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInArray(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& sourceSchema,
    const ValueSchema& itemSchema) const {
    auto result = resolveTypeReferences(currentKey, resolveType, itemSchema);
    if (VALDI_UNLIKELY(!result)) {
        return result.moveError();
    }

    auto itemOutput = result.moveValue();

    if (itemOutput.didChange) {
        return ValueSchemaRegistryResolveOutput(ValueSchema::array(itemOutput.schema), true);
    } else {
        return ValueSchemaRegistryResolveOutput(sourceSchema, false);
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInOutcome(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& sourceSchema,
    const ValueSchema& itemSchema,
    const ValueSchema& errorSchema) const {
    auto result = resolveTypeReferences(currentKey, resolveType, itemSchema);
    if (VALDI_UNLIKELY(!result)) {
        return result.moveError();
    }
    auto errResult = resolveTypeReferences(currentKey, resolveType, errorSchema);
    if (VALDI_UNLIKELY(!errResult)) {
        return errResult.moveError();
    }
    auto itemOutput = result.moveValue();
    auto errOutput = errResult.moveValue();

    if (itemOutput.didChange || errOutput.didChange) {
        return ValueSchemaRegistryResolveOutput(ValueSchema::outcome(itemOutput.schema, errOutput.schema), true);
    } else {
        return ValueSchemaRegistryResolveOutput(sourceSchema, false);
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInPromise(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& sourceSchema,
    const ValueSchema& valueSchema) const {
    auto result = resolveTypeReferences(currentKey, resolveType, valueSchema);
    if (VALDI_UNLIKELY(!result)) {
        return result.moveError();
    }

    auto itemOutput = result.moveValue();

    if (itemOutput.didChange) {
        return ValueSchemaRegistryResolveOutput(ValueSchema::promise(itemOutput.schema), true);
    } else {
        return ValueSchemaRegistryResolveOutput(sourceSchema, false);
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInMap(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& sourceSchema) const {
    const auto* map = sourceSchema.getMap();

    auto keyResult = resolveTypeReferences(currentKey, resolveType, map->getKey());
    if (VALDI_UNLIKELY(!keyResult)) {
        return keyResult.moveError();
    }

    auto valueResult = resolveTypeReferences(currentKey, resolveType, map->getValue());
    if (VALDI_UNLIKELY(!valueResult)) {
        return valueResult.moveError();
    }

    auto keyOutput = keyResult.moveValue();
    auto valueOutput = valueResult.moveValue();

    if (keyOutput.didChange || valueOutput.didChange) {
        return ValueSchemaRegistryResolveOutput(
            ValueSchema::map(std::move(keyOutput.schema), std::move(valueOutput.schema)), true);
    } else {
        return ValueSchemaRegistryResolveOutput(sourceSchema, false);
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInES6Map(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& sourceSchema) const {
    const auto* map = sourceSchema.getES6Map();

    auto keyResult = resolveTypeReferences(currentKey, resolveType, map->getKey());
    if (VALDI_UNLIKELY(!keyResult)) {
        return keyResult.moveError();
    }

    auto valueResult = resolveTypeReferences(currentKey, resolveType, map->getValue());
    if (VALDI_UNLIKELY(!valueResult)) {
        return valueResult.moveError();
    }

    auto keyOutput = keyResult.moveValue();
    auto valueOutput = valueResult.moveValue();

    if (keyOutput.didChange || valueOutput.didChange) {
        return ValueSchemaRegistryResolveOutput(
            ValueSchema::es6map(std::move(keyOutput.schema), std::move(valueOutput.schema)), true);
    } else {
        return ValueSchemaRegistryResolveOutput(sourceSchema, false);
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveTypeReferencesInES6Set(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchema& sourceSchema) const {
    const auto* set = sourceSchema.getES6Set();

    auto keyResult = resolveTypeReferences(currentKey, resolveType, set->getKey());
    if (VALDI_UNLIKELY(!keyResult)) {
        return keyResult.moveError();
    }

    auto keyOutput = keyResult.moveValue();

    if (keyOutput.didChange) {
        return ValueSchemaRegistryResolveOutput(ValueSchema::es6set(std::move(keyOutput.schema)), true);
    } else {
        return ValueSchemaRegistryResolveOutput(sourceSchema, false);
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::resolveSchemaForGenericType(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const GenericValueSchemaTypeReference& generic,
    bool asBoxed) const {
    SmallVector<ValueSchema, 8> resolvedTypeArguments;
    resolvedTypeArguments.reserve(generic.getTypeArgumentsSize());

    for (size_t i = 0; i < generic.getTypeArgumentsSize(); i++) {
        auto result =
            resolveTypeReferences(currentKey, ValueSchemaRegistryResolveType::Key, generic.getTypeArgument(i));

        if (VALDI_UNLIKELY(!result)) {
            return result.error().rethrow(STRING_FORMAT("Could not resolve type argument at position {}", i));
        }

        resolvedTypeArguments.emplace_back(std::move(result.value().schema));
    }

    auto newSchemaKey = ValueSchema::genericTypeReference(
        generic.getType(), resolvedTypeArguments.data(), resolvedTypeArguments.size());
    if (asBoxed) {
        newSchemaKey = newSchemaKey.asBoxed();
    }

    switch (resolveType) {
        case ValueSchemaRegistryResolveType::Key:
            return ValueSchemaRegistryResolveOutput(newSchemaKey, true);
        case ValueSchemaRegistryResolveType::Schema: {
            auto newRegistryKey = ValueSchemaRegistryKey(newSchemaKey);
            auto schemaResult = getResolvedSchemaForTypeReference(
                newRegistryKey, ValueSchemaRegistryResolveType::Schema, generic.getType(), asBoxed);
            if (!schemaResult) {
                return schemaResult.moveError();
            }

            auto schema = std::move(schemaResult.value().schema);
            if (!schema.isSchemaReference()) {
                // Fully resolved Schema, no need to resolve generics
                return ValueSchemaRegistryResolveOutput(std::move(schema), true);
            }

            if (_registry == nullptr) {
                return Error("Cannot resolve schema without a registry instance");
            }

            auto existingSchemaReference = _registry->getSchemaReferenceForTypeKey(newRegistryKey);
            if (existingSchemaReference != nullptr) {
                // Generic already instantiated, return it now
                return ValueSchemaRegistryResolveOutput(getSchemaFromReference(existingSchemaReference), true);
            }

            // No existing schema for this generic type, instantiating now

            const auto* schemaReference = schema.getSchemaReference();
            auto newSchema = schemaReference->getSchema();

            auto identifier = _registry->registerSchema(newRegistryKey, newSchema);
            existingSchemaReference = _registry->getSchemaReferenceForSchemaIdentifier(identifier);

            schemaResult = resolveTypeReferences(newRegistryKey, ValueSchemaRegistryResolveType::Schema, newSchema);
            if (!schemaResult) {
                _registry->unregisterSchema(identifier);
                return schemaResult.moveError();
            }

            _registry->updateSchema(identifier, schemaResult.value().schema);

            return ValueSchemaRegistryResolveOutput(getSchemaFromReference(existingSchemaReference), true);
        }
    }
}

Result<ValueSchemaRegistryResolveOutput> ValueSchemaTypeResolver::getResolvedSchemaForTypeReference(
    const ValueSchemaRegistryKey& currentKey,
    ValueSchemaRegistryResolveType resolveType,
    const ValueSchemaTypeReference& typeReference,
    bool asBoxed) const {
    if (typeReference.isPositional()) {
        auto typeArgument = getTypeArgumentFromSchemaKey(currentKey, typeReference.getPosition());
        if (VALDI_UNLIKELY(!typeArgument)) {
            return typeArgument.moveError();
        }

        auto result = resolveTypeReferences(currentKey, resolveType, typeArgument.value());
        if (VALDI_UNLIKELY(!result)) {
            return result.moveError();
        }

        if (asBoxed) {
            return ValueSchemaRegistryResolveOutput(ValueSchema::boxed(std::move(result.value().schema)), true);
        } else {
            return ValueSchemaRegistryResolveOutput(std::move(result.value().schema), true);
        }
    }

    auto schemaKey = ValueSchema::typeReference(
        ValueSchemaTypeReference::named(typeReference.getTypeHint(), typeReference.getName()));
    if (asBoxed) {
        schemaKey = schemaKey.asBoxed();
    }

    switch (resolveType) {
        case ValueSchemaRegistryResolveType::Key:
            return ValueSchemaRegistryResolveOutput(schemaKey, true);
        case ValueSchemaRegistryResolveType::Schema: {
            if (_registry == nullptr) {
                return Error("Cannot resolve schema from type references without a registry set");
            }

            ValueSchemaRegistryKey key(schemaKey);
            auto resolved = _registry->getOrResolveSchemaReferenceForTypeKey(key);

            if (VALDI_UNLIKELY(!resolved)) {
                return resolved.error().rethrow(
                    STRING_FORMAT("Could not resolve type reference {}", typeReference.toString()));
            }

            return ValueSchemaRegistryResolveOutput(getSchemaFromReference(resolved.value()), true);
        }
    }
}

ValueSchemaRegistry* ValueSchemaTypeResolver::getRegistry() const {
    return _registry;
}

static bool shouldUseSchemaReference(const ValueSchema& schema) {
    // We avoid schema references if the schema is a simple type
    if (schema.isUntyped() || schema.isString() || schema.isLongInteger() || schema.isBigInt() || schema.isDouble() ||
        schema.isBoolean() || schema.isValueTypedArray()) {
        return false;
    }

    if (schema.isArray()) {
        return shouldUseSchemaReference(schema.getArrayItemSchema());
    }

    return true;
}

ValueSchema ValueSchemaTypeResolver::getSchemaFromReference(const Ref<ValueSchemaReference>& schemaReference) {
    auto schema = schemaReference->getSchema();
    if (shouldUseSchemaReference(schema)) {
        return ValueSchema::schemaReference(schemaReference);
    } else {
        // Avoid schema reference indirection if the schema reference doesn't refer to a value map
        return schema;
    }
}

} // namespace Valdi
