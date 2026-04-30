//
//  ValueSchemaRegistry.cpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 1/17/23.
//

#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include <sstream>

namespace Valdi {

class ValueSchemaRegistryReference : public ValueSchemaReference {
public:
    ValueSchemaRegistryReference(ValueSchemaRegistry* registry,
                                 const ValueSchemaRegistryKey& typeKey,
                                 ValueSchemaRegistrySchemaIdentifier identifier)
        : _registry(weakRef(registry)), _key(typeKey.getSchemaKey()), _identifier(identifier) {}

    ~ValueSchemaRegistryReference() override = default;

    ValueSchemaRegistrySchemaIdentifier getIdentifier() const {
        return _identifier;
    }

    const ValueSchemaRegistryKey& getRegistryKey() const {
        return _key;
    }

    ValueSchema getKey() const final {
        return _key.getSchemaKey();
    }

    ValueSchema getSchema() const final {
        auto registry = _registry.lock();
        if (registry == nullptr) {
            return ValueSchema::voidType();
        }

        return registry->getSchemaForIdentifier(_identifier);
    }

private:
    Weak<ValueSchemaRegistry> _registry;
    ValueSchemaRegistryKey _key;
    ValueSchemaRegistrySchemaIdentifier _identifier;
};

ValueSchemaRegistry::ValueSchemaRegistry() = default;
ValueSchemaRegistry::~ValueSchemaRegistry() = default;

Ref<ValueSchemaReference> ValueSchemaRegistry::getSchemaReferenceForTypeKey(
    const ValueSchemaRegistryKey& typeKey) const {
    std::lock_guard<std::recursive_mutex> guard(_mutex);
    const auto& it = _entryIndexByKey.find(typeKey);
    if (it == _entryIndexByKey.end()) {
        return nullptr;
    }

    return _entries[it->second].reference;
}

Ref<ValueSchemaReference> ValueSchemaRegistry::getSchemaReferenceForSchemaIdentifier(
    ValueSchemaRegistrySchemaIdentifier identifier) const {
    std::lock_guard<std::recursive_mutex> guard(_mutex);
    SC_ASSERT(identifier < _entries.size());
    return _entries[identifier].reference;
}

std::optional<ValueSchema> ValueSchemaRegistry::getSchemaForTypeKey(const ValueSchemaRegistryKey& typeKey) const {
    auto reference = getSchemaReferenceForTypeKey(typeKey);
    if (reference == nullptr) {
        return std::nullopt;
    }

    return {reference->getSchema()};
}

std::optional<ValueSchema> ValueSchemaRegistry::getSchemaForTypeName(const StringBox& typeName) const {
    auto key = ValueSchemaRegistryKey(ValueSchema::typeReference(ValueSchemaTypeReference::named(typeName)));
    return getSchemaForTypeKey(key);
}

static ValueSchema simplifyTypeReferences(const ValueSchema& schema, bool* changed) {
    if (schema.isTypeReference()) {
        auto typeReference = schema.getTypeReference();
        if (typeReference.getTypeHint() != ValueSchemaTypeReferenceTypeHintUnknown) {
            *changed = true;
            return ValueSchema::typeReference(
                ValueSchemaTypeReference::named(ValueSchemaTypeReferenceTypeHintUnknown, typeReference.getName()));
        }
    } else if (schema.isGenericTypeReference()) {
        const auto* genericTypeReference = schema.getGenericTypeReference();
        auto typeChanged = false;
        auto newGenericType =
            simplifyTypeReferences(ValueSchema::typeReference(genericTypeReference->getType()), &typeChanged);
        SmallVector<ValueSchema, 3> newTypeArguments;

        for (size_t i = 0; i < genericTypeReference->getTypeArgumentsSize(); i++) {
            newTypeArguments.emplace_back(
                simplifyTypeReferences(genericTypeReference->getTypeArgument(i), &typeChanged));
        }

        if (typeChanged) {
            *changed = true;
            return ValueSchema::genericTypeReference(
                newGenericType.getTypeReference(), newTypeArguments.data(), newTypeArguments.size());
        }
    }

    return schema;
}

static ValueSchemaRegistryKey simplifyTypeKey(const ValueSchemaRegistryKey& typeKey, bool* changed) {
    auto newSchema = simplifyTypeReferences(typeKey.getSchemaKey(), changed);
    if (*changed) {
        return ValueSchemaRegistryKey(newSchema);
    } else {
        return typeKey;
    }
}

Result<Ref<ValueSchemaReference>> ValueSchemaRegistry::getOrResolveSchemaReferenceForTypeKey(
    const ValueSchemaRegistryKey& typeKey) {
    std::lock_guard<std::recursive_mutex> guard(_mutex);
    const auto& it = _entryIndexByKey.find(typeKey);
    if (it != _entryIndexByKey.end()) {
        const auto& entry = _entries[it->second];
        if (entry.reference != nullptr) {
            return Ref<ValueSchemaReference>(entry.reference);
        }
    }

    auto simplified = false;
    auto simplifiedKey = simplifyTypeKey(typeKey, &simplified);

    if (simplified) {
        // If we had a converted type reference, we generate an entry on the fly
        // that links to the unconverted schema
        auto unknownTypeReference = getOrResolveSchemaReferenceForTypeKey(simplifiedKey);
        if (!unknownTypeReference) {
            return unknownTypeReference.moveError();
        }

        auto schemaToRegister = unknownTypeReference.value()->getSchema();
        if (typeKey.getSchemaKey().isBoxed()) {
            schemaToRegister = schemaToRegister.asBoxed();
        }

        registerSchema(typeKey, schemaToRegister);

        return getSchemaPostRegistration(typeKey);
    }

    if (_listener == nullptr) {
        return Error("Type not registered in ValueSchemaRegistry");
    }

    auto result = _listener->resolveSchemaIdentifierForSchemaKey(*this, typeKey);
    if (VALDI_UNLIKELY(!result)) {
        return result.moveError();
    }

    return getSchemaPostRegistration(typeKey);
}

Ref<ValueSchemaReference> ValueSchemaRegistry::getSchemaPostRegistration(const ValueSchemaRegistryKey& typeKey) const {
    const auto& updatedIt = _entryIndexByKey.find(typeKey);
    SC_ASSERT(updatedIt != _entryIndexByKey.end());
    const auto& entry = _entries[updatedIt->second];
    SC_ASSERT(entry.reference != nullptr);

    return Ref<ValueSchemaReference>(entry.reference);
}

ValueSchemaRegistrySchemaIdentifier ValueSchemaRegistry::registerSchema(const ValueSchema& schema) {
    if (schema.isClass()) {
        return registerSchema(schema.getClass()->getClassName(), schema);
    } else if (schema.isEnum()) {
        return registerSchema(schema.getEnum()->getName(), schema);
    }

    SC_ASSERT_FAIL("Invalid schema type");
    return 0;
}

ValueSchemaRegistrySchemaIdentifier ValueSchemaRegistry::registerSchema(const StringBox& typeName,
                                                                        const ValueSchema& schema) {
    return registerSchema(ValueSchema::typeReference(ValueSchemaTypeReference::named(typeName)), schema);
}

ValueSchemaRegistrySchemaIdentifier ValueSchemaRegistry::registerSchema(const ValueSchema& schemaKey,
                                                                        const ValueSchema& schema) {
    return registerSchema(ValueSchemaRegistryKey(schemaKey), schema);
}

ValueSchemaRegistrySchemaIdentifier ValueSchemaRegistry::registerSchema(const ValueSchemaRegistryKey& schemaKey,
                                                                        const ValueSchema& schema) {
    std::lock_guard<std::recursive_mutex> guard(_mutex);

    const auto& it = _entryIndexByKey.find(schemaKey);
    if (it != _entryIndexByKey.end() && _entries[it->second].schema == schema) {
        return it->second;
    }

    auto entryIndex = _entries.size();
    auto& entry = _entries.emplace_back();
    entry.schema = schema;
    entry.reference = makeShared<ValueSchemaRegistryReference>(this, schemaKey, entryIndex);

    _entryIndexByKey[schemaKey] = entryIndex;

    return entryIndex;
}

void ValueSchemaRegistry::unregisterSchema(ValueSchemaRegistrySchemaIdentifier identifier) {
    auto guard = lock();
    SC_ASSERT(identifier < _entries.size());

    // The array indexes must remain consistent so we can't remove the item.
    auto& entry = _entries[identifier];
    entry.schema = ValueSchema::voidType();

    if (entry.reference != nullptr) {
        auto typeKey = entry.reference->getRegistryKey();
        entry.reference = nullptr;
        const auto& it = _entryIndexByKey.find(typeKey);
        if (it != _entryIndexByKey.end()) {
            _entryIndexByKey.erase(it);
        }
    }
}

ValueSchema ValueSchemaRegistry::getSchemaForIdentifier(ValueSchemaRegistrySchemaIdentifier index) const {
    std::lock_guard<std::recursive_mutex> guard(_mutex);
    if (index >= _entries.size()) {
        return ValueSchema::voidType();
    }

    return _entries[index].schema;
}

std::optional<RegisteredValueSchema> ValueSchemaRegistry::getSchemaAndKeyForIdentifier(
    ValueSchemaRegistrySchemaIdentifier index) const {
    std::lock_guard<std::recursive_mutex> guard(_mutex);
    if (index >= _entries.size()) {
        return std::nullopt;
    }

    const auto& entry = _entries[index];
    if (entry.reference == nullptr) {
        return std::nullopt;
    }

    return {RegisteredValueSchema(entry.reference->getRegistryKey(), entry.schema)};
}

void ValueSchemaRegistry::updateSchema(ValueSchemaRegistrySchemaIdentifier identifier, const ValueSchema& schema) {
    auto guard = lock();
    SC_ASSERT(identifier < _entries.size());
    _entries[identifier].schema = schema;
}

bool ValueSchemaRegistry::updateSchemaIfKeyExists(const ValueSchemaRegistryKey& schemaKey, const ValueSchema& schema) {
    auto guard = lock();
    const auto& it = _entryIndexByKey.find(schemaKey);
    if (it == _entryIndexByKey.end()) {
        return false;
    }
    _entries[it->second].schema = schema;
    return true;
}

std::unique_lock<std::recursive_mutex> ValueSchemaRegistry::lock() const {
    return std::unique_lock<std::recursive_mutex>(_mutex);
}

void ValueSchemaRegistry::setListener(const Ref<ValueSchemaRegistryListener>& listener) {
    auto guard = lock();
    _listener = listener;
}

std::vector<ValueSchema> ValueSchemaRegistry::getAllSchemas() const {
    std::lock_guard<std::recursive_mutex> guard(_mutex);

    std::vector<ValueSchema> schemas;
    schemas.reserve(_entries.size());
    for (const auto& entry : _entries) {
        if (entry.reference == nullptr) {
            continue;
        }
        schemas.emplace_back(entry.schema);
    }
    return schemas;
}

std::vector<ValueSchema> ValueSchemaRegistry::getAllSchemaKeys() const {
    std::lock_guard<std::recursive_mutex> guard(_mutex);

    std::vector<ValueSchema> schemas;
    schemas.reserve(_entries.size());
    for (const auto& entry : _entries) {
        if (entry.reference == nullptr) {
            continue;
        }
        schemas.emplace_back(entry.reference->getKey());
    }
    return schemas;
}

Ref<ValueSchemaRegistry> ValueSchemaRegistry::sharedInstance() {
    static auto kInstance = makeShared<ValueSchemaRegistry>();

    return kInstance;
}

} // namespace Valdi

namespace std {

std::size_t hash<Valdi::ValueSchemaRegistryKey>::operator()(const Valdi::ValueSchemaRegistryKey& k) const noexcept {
    return k.hash();
}

} // namespace std
