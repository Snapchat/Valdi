//
//  AndroidValueMarshallerRegistry.cpp
//  valdi-android
//
//  Created by Simon Corsin on 2/14/23.
//

#include "valdi/android/AndroidValueMarshallerRegistry.hpp"
#include "valdi/android/JavaClassDelegate.hpp"
#include "valdi/android/JavaEnumClassDelegate.hpp"
#include "valdi/android/JavaInterfaceClassDelegate.hpp"
#include "valdi/android/JavaObjectClassDelegate.hpp"
#include "valdi/android/JavaUntypedClassDelegate.hpp"
#include "valdi/android/JavaValueDelegate.hpp"
#include "valdi/android/ValdiMarshallableObjectDescriptorJavaClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/TextParser.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/jni/JavaClass.hpp"

namespace ValdiAndroid {

class ValueSchemaRegistryListenerImpl : public Valdi::ValueSchemaRegistryListener {
public:
    explicit ValueSchemaRegistryListenerImpl(AndroidValueMarshallerRegistry& registry) : _registry(registry) {}

    Valdi::Result<Valdi::Void> resolveSchemaIdentifierForSchemaKey(
        Valdi::ValueSchemaRegistry& registry, const Valdi::ValueSchemaRegistryKey& registryKey) final {
        if (!registryKey.getSchemaKey().isTypeReference()) {
            return Valdi::Error("Not a type reference");
        }

        auto typeReference = registryKey.getSchemaKey().getTypeReference();
        if (!typeReference.isNamed()) {
            return Valdi::Error("Not a named type reference");
        }

        auto result = _registry.getOrCreateRegisteredMarshallableClass(typeReference.getName());
        if (!result) {
            return result.moveError();
        }
        return Valdi::Void();
    }

private:
    AndroidValueMarshallerRegistry& _registry;
};

FieldNameMap::FieldNameMap() = default;
FieldNameMap::~FieldNameMap() = default;

Valdi::StringBox FieldNameMap::getFieldName(size_t fieldIndex,
                                            const Valdi::StringBox& propertyName,
                                            FieldNameMap::FieldType fieldType) const {
    const auto& it = _map.find(fieldIndex);
    if (it != _map.end()) {
        return it->second;
    }

    switch (fieldType) {
        case FieldType::Class:
            return propertyName.prepend("_");
        case FieldType::InterfaceMethod:
            return propertyName;
        case FieldType::InterfaceProperty: {
            if (propertyName.hasPrefix("is") && propertyName.length() > 2 &&
                static_cast<bool>(std::isupper(propertyName[2]))) {
                return propertyName;
            } else {
                auto str = propertyName.toStringView();
                std::string getterName = "get";
                getterName += std::toupper(str[0]);
                getterName += str.substr(1);

                return Valdi::StringCache::getGlobal().makeString(std::move(getterName));
            }
        } break;
        case FieldType::EnumCaseValue:
            return propertyName;
    }
}

void FieldNameMap::appendFieldName(size_t fieldIndex, const Valdi::StringBox& fieldName) {
    _map[fieldIndex] = fieldName;
}

AndroidValueMarshallerRegistry::AndroidValueMarshallerRegistry()
    : _schemaRegistry(Valdi::makeShared<Valdi::ValueSchemaRegistry>()),
      _valueMarshallerRegistry(Valdi::ValueSchemaTypeResolver(_schemaRegistry.get()),
                               Valdi::makeShared<JavaValueDelegate>(*this),
                               Valdi::DispatchQueue::create(STRING_LITERAL("Valdi Callback Queue"),
                                                            Valdi::ThreadQoSClass::ThreadQoSClassHigh),
                               STRING_LITERAL("Java")) {
    _schemaRegistry->setListener(Valdi::makeShared<ValueSchemaRegistryListenerImpl>(*this));
    _valueMarshallerRegistry.setListener(this);
    registerUntyped(STRING_LITERAL("java.lang.Object"));
    registerUntyped(STRING_LITERAL("com.snapchat.client.valdi_core.Asset"));
}

AndroidValueMarshallerRegistry::~AndroidValueMarshallerRegistry() = default;

Valdi::ValueSchemaRegistrySchemaIdentifier AndroidValueMarshallerRegistry::registerUntyped(
    const Valdi::StringBox& className) {
    auto schemaKey = Valdi::ValueSchema::typeReference(Valdi::ValueSchemaTypeReference::named(className));
    return _schemaRegistry->registerSchema(schemaKey, Valdi::ValueSchema::untyped());
}

Valdi::Result<Valdi::Value> AndroidValueMarshallerRegistry::marshallObject(const Valdi::StringBox& className,
                                                                           const JavaValue& object) {
    auto classDelegate = getLoadedClassDelegateForClassName(className);
    if (!classDelegate) {
        return classDelegate.moveError();
    }

    Valdi::SimpleExceptionTracker exceptionTracker;
    auto result = classDelegate.value()->getValueMarshaller()->marshall(
        nullptr, object, Valdi::ReferenceInfoBuilder(), exceptionTracker);

    return exceptionTracker.toResult(std::move(result));
}

Valdi::Result<JavaValue> AndroidValueMarshallerRegistry::unmarshallObject(const Valdi::StringBox& className,
                                                                          const Valdi::Value& value) {
    auto classDelegate = getLoadedClassDelegateForClassName(className);
    if (!classDelegate) {
        return classDelegate.moveError();
    }

    Valdi::SimpleExceptionTracker exceptionTracker;
    auto result =
        classDelegate.value()->getValueMarshaller()->unmarshall(value, Valdi::ReferenceInfoBuilder(), exceptionTracker);

    return exceptionTracker.toResult(std::move(result));
}

Valdi::Result<Valdi::Void> AndroidValueMarshallerRegistry::setActiveSchemaInMarshaller(
    const Valdi::StringBox& className, Valdi::Marshaller& marshaller) {
    auto classDelegate = getLoadedClassDelegateForClassName(className);
    if (!classDelegate) {
        return classDelegate.moveError();
    }

    auto* classSchema = classDelegate.value()->getSchema();
    SC_ABORT_UNLESS(classSchema != nullptr, "classDelegate does not have a schema");
    marshaller.setCurrentSchema(Valdi::ValueSchema::cls(Valdi::strongSmallRef(classSchema)));

    return Valdi::Void();
}

Valdi::Result<Valdi::Value> AndroidValueMarshallerRegistry::getEnumValue(const Valdi::StringBox& className,
                                                                         const JavaValue& enumValue) {
    auto classDelegate = getLoadedClassDelegateForClassName(className);
    if (!classDelegate) {
        return classDelegate.moveError();
    }
    auto enumClass = dynamic_cast<JavaEnumClassDelegate*>(classDelegate.value());
    if (enumClass == nullptr) {
        return Valdi::Error(STRING_FORMAT("Class {} is not an enum", className));
    }

    Valdi::SimpleExceptionTracker exceptionTracker;
    return exceptionTracker.toResult(enumClass->enumCaseToValue(enumValue, false, exceptionTracker));
}

Valdi::ValueSchema AndroidValueMarshallerRegistry::getSchemaForInterfacePropertyUnmarshaller(
    const Valdi::ValueSchema& schema) {
    if (!schema.isFunction()) {
        return schema;
    }

    const auto* function = schema.getFunction();

    if (!function->getAttributes().isMethod()) {
        return schema;
    }

    bool changed = false;

    Valdi::SmallVector<Valdi::ValueSchema, 8> parameters;

    for (size_t i = 0; i < function->getParametersSize(); i++) {
        const auto& parameter = function->getParameter(i);

        auto javaValueType = schemaToJavaValueType(parameter);

        if (javaValueType != JavaValueType::Object) {
            // Box primitive types for parameters of methods that get unmarshalled.
            // We represent unmarshalled methods as Function objects, which needs to be
            // boxed.
            parameters.emplace_back(parameter.asBoxed());
            changed = true;
        } else {
            parameters.emplace_back(parameter);
        }
    }

    auto returnValue = function->getReturnValue();
    if (schemaToJavaValueType(returnValue) != JavaValueType::Object) {
        returnValue = returnValue.asBoxed();
        changed = true;
    }

    if (changed) {
        return Valdi::ValueSchema::function(
            function->getAttributes(), returnValue, parameters.data(), parameters.size());
    } else {
        return schema;
    }
}

void AndroidValueMarshallerRegistry::setDescriptorClosureEnabled(bool enabled) {
    // Guard with the same lock that getOrCreateRegisteredMarshallableClass holds when it reads the
    // flag, so this startup write is properly synchronized with marshalling-thread reads.
    auto lock = _schemaRegistry->lock();
    _descriptorClosureEnabled = enabled;
}

void AndroidValueMarshallerRegistry::setLazyFunctionReturnMarshallerEnabled(bool enabled) {
    auto lock = _schemaRegistry->lock();
    _valueMarshallerRegistry.setLazyFunctionReturnMarshallerEnabled(enabled);
}

Valdi::Ref<Valdi::PlatformObjectClassDelegate<JavaValue>> AndroidValueMarshallerRegistry::getObjectClassDelegateForName(
    const Valdi::StringBox& className) {
    auto result = getOrCreateRegisteredMarshallableClass(className);
    SC_ASSERT(result);

    auto javaClass = dynamic_cast<JavaObjectClassDelegate*>(result.value());

    SC_ASSERT(javaClass != nullptr);

    return Valdi::Ref<JavaObjectClassDelegate>(javaClass);
}

Valdi::Ref<Valdi::PlatformObjectClassDelegate<JavaValue>> AndroidValueMarshallerRegistry::
    getInterfaceClassDelegateForName(const Valdi::StringBox& className) {
    auto result = getOrCreateRegisteredMarshallableClass(className);
    SC_ASSERT(result);

    auto javaClass = dynamic_cast<JavaInterfaceClassDelegate*>(result.value());

    SC_ASSERT(javaClass != nullptr);

    return Valdi::Ref<JavaInterfaceClassDelegate>(javaClass);
}

Valdi::Ref<Valdi::PlatformEnumClassDelegate<JavaValue>> AndroidValueMarshallerRegistry::getEnumClassDelegateForName(
    const Valdi::StringBox& className) {
    auto result = getOrCreateRegisteredMarshallableClass(className);
    SC_ASSERT(result);

    auto enumClass = dynamic_cast<JavaEnumClassDelegate*>(result.value());

    SC_ASSERT(enumClass != nullptr);

    return Valdi::Ref<JavaEnumClassDelegate>(enumClass);
}

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::getLoadedClassDelegateForClassName(
    const Valdi::StringBox& className) {
    // Force keep instance during the duration of this call
    auto selfRef = Valdi::strongSmallRef(this);
    auto lock = _schemaRegistry->lock();
    auto marshallableClassResult = getOrCreateRegisteredMarshallableClass(className);
    if (!marshallableClassResult) {
        return marshallableClassResult.moveError();
    }

    if (marshallableClassResult.value()->getValueMarshaller() == nullptr) {
        VALDI_TRACE_META("Valdi.resolveValueMarshaller", className);
        auto schemaAndKey =
            _schemaRegistry->getSchemaAndKeyForIdentifier(marshallableClassResult.value()->getIdentifier());
        SC_ABORT_UNLESS(schemaAndKey.has_value(), "Cannot resolve schema");

        Valdi::SimpleExceptionTracker exceptionTracker;
        auto valueMarshallerAndSchema = _valueMarshallerRegistry.getValueMarshaller(
            schemaAndKey.value().schemaKey, schemaAndKey.value().schema, exceptionTracker);
        if (!exceptionTracker) {
            return exceptionTracker.extractError();
        }

        marshallableClassResult.value()->setValueMarshaller(valueMarshallerAndSchema.valueMarshaller.get());
        marshallableClassResult.value()->setSchema(valueMarshallerAndSchema.schema.getClassRef().get());
    }

    return marshallableClassResult;
}

static Valdi::Result<Valdi::Void> parsePropertyReplacements(const std::string_view& input, FieldNameMap& output) {
    Valdi::TextParser parser(input);

    while (!parser.isAtEnd()) {
        auto fieldIndex = parser.parseInt();
        if (!fieldIndex) {
            return parser.getError();
        }
        if (!parser.parse(":'")) {
            return parser.getError();
        }

        auto fieldName = parser.readUntilCharacterAndParse('\'');
        if (!fieldName) {
            return parser.getError();
        }

        output.appendFieldName(static_cast<size_t>(fieldIndex.value()),
                               Valdi::StringCache::getGlobal().makeString(fieldName.value()));

        parser.tryParse(',');
    }

    return Valdi::Void();
}

FieldNameMap AndroidValueMarshallerRegistry::toFieldMap(const std::string& propertyReplacements) {
    FieldNameMap out;

    if (!propertyReplacements.empty()) {
        auto result = parsePropertyReplacements(propertyReplacements, out);
        SC_ASSERT(result, result.description());
    }

    return out;
}

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::registerObjectClassDelegate(
    const Valdi::StringBox& className,
    const JavaClass& javaClass,
    const std::string_view& schemaSuffix,
    const FieldNameMap& fieldMap,
    const std::vector<std::string>& typeReferenceNames) {
    auto registeredSchema = parseAndRegisterSchema(fmt::format("c'{}'", className), schemaSuffix, typeReferenceNames);
    if (!registeredSchema) {
        return registeredSchema.moveError();
    }

    const auto* schemaClass = registeredSchema.value().schema.getClass();
    SC_ASSERT(schemaClass != nullptr);

    std::vector<Valdi::ValueSchema> constructorParameterTypes;
    constructorParameterTypes.reserve(schemaClass->getPropertiesSize());
    for (const auto& property : *schemaClass) {
        constructorParameterTypes.emplace_back(property.schema);
    }

    auto constructor =
        javaClass.getConstructor(constructorParameterTypes.data(), constructorParameterTypes.size(), true);

    auto registeredClass = JavaObjectClassDelegate::make(
        registeredSchema.value().identifier, javaClass.getClass(), constructor, schemaClass->getPropertiesSize());

    // Disabled in production to avoid per-member trace overhead (fires once per property of every
    // marshalled class). Uncomment for local profiling of JNI member binding.
    // VALDI_TRACE_META("Valdi.bindClassMembers", className);
    size_t propertyIndex = 0;
    for (const auto& property : *schemaClass) {
        auto fieldName = fieldMap.getFieldName(propertyIndex, property.name, FieldNameMap::FieldType::Class);
        auto field = javaClass.getField(fieldName.getCStr(), property.schema, true);

        registeredClass->setField(propertyIndex, field);

        propertyIndex++;
    }

    _registeredClassByName[className] = registeredClass;

    return registeredClass.get();
}

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::registerInterfaceClassDelegate(
    const Valdi::StringBox& className,
    const JavaClass& javaClass,
    const std::string_view& schemaSuffix,
    const FieldNameMap& fieldMap,
    const JavaClass& proxyJavaClass,
    const std::vector<std::string>& typeReferenceNames) {
    auto registeredSchema = parseAndRegisterSchema(fmt::format("c+'{}'", className), schemaSuffix, typeReferenceNames);
    if (!registeredSchema) {
        return registeredSchema.moveError();
    }

    const auto* schemaClass = registeredSchema.value().schema.getClass();
    SC_ASSERT(schemaClass != nullptr);

    std::vector<Valdi::ValueSchema> constructorParameterTypes;
    constructorParameterTypes.reserve(schemaClass->getPropertiesSize());
    for (const auto& property : *schemaClass) {
        constructorParameterTypes.emplace_back(property.schema);
    }

    auto constructor =
        proxyJavaClass.getConstructor(constructorParameterTypes.data(), constructorParameterTypes.size(), true);

    auto registeredClass = JavaInterfaceClassDelegate::make(registeredSchema.value().identifier,
                                                            proxyJavaClass.getClass(),
                                                            javaClass.getClass(),
                                                            constructor,
                                                            schemaClass->getPropertiesSize());

    // Disabled in production to avoid per-member trace overhead (fires once per method of every
    // marshalled interface). Uncomment for local profiling of JNI member binding.
    // VALDI_TRACE_META("Valdi.bindClassMembers", className);
    size_t methodIndex = 0;
    for (const auto& property : *schemaClass) {
        const auto& propertyType = property.schema;

        if (propertyType.isFunction()) {
            const auto* functionSchema = property.schema.getFunction();

            auto methodName =
                fieldMap.getFieldName(methodIndex, property.name, FieldNameMap::FieldType::InterfaceMethod);
            auto method = javaClass.getMethod(methodName.getCStr(), *functionSchema, true);
            auto javaMethod = method.toReflectedMethod(javaClass.getClass(), false);

            registeredClass->setMethod(methodIndex, method, javaMethod.get(), propertyType.isOptional(), false);
        } else {
            auto methodName =
                fieldMap.getFieldName(methodIndex, property.name, FieldNameMap::FieldType::InterfaceProperty);
            auto method = javaClass.getMethod(methodName.getCStr(), propertyType, nullptr, 0, true);
            registeredClass->setMethod(methodIndex, method, nullptr, propertyType.isOptional(), true);
        }

        methodIndex++;
    }

    _registeredClassByName[className] = registeredClass;

    return registeredClass.get();
}

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::registerStringEnumClassDelegate(
    const Valdi::StringBox& className,
    const JavaClass& javaClass,
    const std::string_view& schemaSuffix,
    const FieldNameMap& fieldMap) {
    return registerEnumClassDelegate('s', className, javaClass, schemaSuffix, fieldMap);
}

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::registerIntEnumClassDelegate(
    const Valdi::StringBox& className,
    const JavaClass& javaClass,
    const std::string_view& schemaSuffix,
    const FieldNameMap& fieldMap) {
    return registerEnumClassDelegate('i', className, javaClass, schemaSuffix, fieldMap);
}

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::registerEnumClassDelegate(
    char enumType,
    const Valdi::StringBox& className,
    const JavaClass& javaClass,
    const std::string_view& schemaSuffix,
    const FieldNameMap& fieldMap) {
    auto registeredSchema =
        parseAndRegisterSchema(fmt::format("e<{}>'{}'", enumType, className), schemaSuffix, std::vector<std::string>{});
    if (!registeredSchema) {
        return registeredSchema.moveError();
    }

    auto enumSchema = registeredSchema.value().schema.getEnumRef();
    SC_ASSERT(enumSchema != nullptr);

    auto registeredClass =
        JavaEnumClassDelegate::make(registeredSchema.value().identifier, javaClass.getClass(), enumSchema);

    auto enumTypeReference = Valdi::ValueSchema::typeReference(Valdi::ValueSchemaTypeReference::named(className));

    size_t enumIndex = 0;
    for (const auto& enumCase : *enumSchema) {
        auto enumName = fieldMap.getFieldName(enumIndex, enumCase.name, FieldNameMap::FieldType::EnumCaseValue);
        auto enumField = javaClass.getStaticField(enumName.getCStr(), enumTypeReference, true);
        auto enumValue = enumField.getFieldValue(reinterpret_cast<jobject>(javaClass.getClass()));

        registeredClass->setEnumCase(enumIndex, enumValue);

        enumIndex++;
    }

    _registeredClassByName[className] = registeredClass;

    return registeredClass.get();
}

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::registerUntypedClassDelegate(
    const Valdi::StringBox& className, const JavaClass& javaClass) {
    auto identifier = registerUntyped(className);
    auto registeredClass = Valdi::makeShared<JavaUntypedClassDelegate>(identifier, javaClass.getClass());

    _registeredClassByName[className] = registeredClass;

    return registeredClass.get();
}

static Valdi::Result<Valdi::Void> convertTypeReferences(const std::vector<std::string>& typeReferences,
                                                        const std::string_view& input,
                                                        std::string& output) {
    Valdi::TextParser parser(input);
    while (!parser.isAtEnd()) {
        output += parser.readUntilCharacter('[');

        if (!parser.isAtEnd()) {
            if (!parser.parse('[')) {
                return parser.getError();
            }
            auto index = parser.parseInt();
            if (!index) {
                return parser.getError();
            }
            if (!parser.parse(']')) {
                return parser.getError();
            }

            auto sizeTIndex = static_cast<size_t>(index.value());

            if (sizeTIndex >= typeReferences.size()) {
                return Valdi::Error("Out of bounds index");
            }

            output += typeReferences[sizeTIndex];
        }
    }

    return Valdi::Void();
}

Valdi::Result<AndroidValueMarshallerRegistry::RegisteredSchema> AndroidValueMarshallerRegistry::parseAndRegisterSchema(
    const std::string& prefix, const std::string_view& suffix, const std::vector<std::string>& typeReferenceNames) {
    std::string schemaString;

    schemaString += prefix;
    schemaString += "{";

    // convertTypeReferences substitutes [N] placeholders with typeReferenceNames[N]. With no refs
    // (e.g. enums) there are no placeholders to substitute, and the suffix may legitimately contain a
    // '[' (e.g. a string-enum value like '[x]') that convertTypeReferences would misparse as a
    // placeholder and fail on. Copy verbatim in that case, matching the pre-batch isNull guard.
    if (typeReferenceNames.empty()) {
        schemaString += suffix;
    } else {
        auto result = convertTypeReferences(typeReferenceNames, suffix, schemaString);
        SC_ASSERT(result, result.description());
    }

    schemaString += "}";

    auto parsedSchema = Valdi::ValueSchema::parse(schemaString);
    if (!parsedSchema) {
        return parsedSchema.moveError();
    }

    auto identifier = _schemaRegistry->registerSchema(parsedSchema.value());

    return AndroidValueMarshallerRegistry::RegisteredSchema(identifier, parsedSchema.value());
}

namespace {

constexpr int kTypeClass = 1;
constexpr int kTypeInterface = 2;
constexpr int kTypeStringEnum = 3;
constexpr int kTypeIntEnum = 4;
constexpr int kTypeUntyped = 5;
constexpr uint8_t kDescriptorClosureFormatVersion = 2;

std::vector<std::string> javaStringArrayToVector(const JavaValue& array) {
    std::vector<std::string> out;
    if (!array.isNull()) {
        JavaObjectArray iterator(array.getObjectArray());
        auto size = iterator.size();
        out.reserve(size);
        for (size_t i = 0; i < size; i++) {
            auto element = iterator.getObject(i);
            out.emplace_back(toStdString(JavaEnv(), reinterpret_cast<jstring>(element.get())));
        }
    }
    return out;
}

} // namespace

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::registerParsedDescriptor(
    const Valdi::StringBox& className,
    const JavaClass& javaClass,
    int type,
    const std::string_view& schemaSuffix,
    const FieldNameMap& fieldMap,
    const JavaClass* proxyJavaClass,
    const std::vector<std::string>& typeReferenceNames) {
    if (type == kTypeClass) {
        return registerObjectClassDelegate(className, javaClass, schemaSuffix, fieldMap, typeReferenceNames);
    } else if (type == kTypeInterface) {
        SC_ASSERT(proxyJavaClass != nullptr, "Interface marshallable requires a proxy class");
        return registerInterfaceClassDelegate(
            className, javaClass, schemaSuffix, fieldMap, *proxyJavaClass, typeReferenceNames);
    } else if (type == kTypeStringEnum) {
        return registerStringEnumClassDelegate(className, javaClass, schemaSuffix, fieldMap);
    } else if (type == kTypeIntEnum) {
        return registerIntEnumClassDelegate(className, javaClass, schemaSuffix, fieldMap);
    } else if (type == kTypeUntyped) {
        return registerUntypedClassDelegate(className, javaClass);
    } else {
        return Valdi::Error(STRING_FORMAT("Invalid marshallable object type '{}'", type));
    }
}

void AndroidValueMarshallerRegistry::parseDescriptorClosure(const uint8_t* data, size_t size) {
    size_t pos = 0;
    auto readU8 = [&](uint8_t& out) -> bool {
        if (pos + 1 > size) {
            return false;
        }
        out = data[pos++];
        return true;
    };
    auto readU16 = [&](uint32_t& out) -> bool {
        if (pos + 2 > size) {
            return false;
        }
        out = static_cast<uint32_t>(data[pos]) | (static_cast<uint32_t>(data[pos + 1]) << 8);
        pos += 2;
        return true;
    };
    auto readU32 = [&](uint32_t& out) -> bool {
        if (pos + 4 > size) {
            return false;
        }
        out = static_cast<uint32_t>(data[pos]) | (static_cast<uint32_t>(data[pos + 1]) << 8) |
              (static_cast<uint32_t>(data[pos + 2]) << 16) | (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return true;
    };
    auto readString = [&](std::string& out) -> bool {
        uint32_t length = 0;
        if (!readU32(length)) {
            return false;
        }
        if (pos + length > size) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(data + pos), length);
        pos += length;
        return true;
    };

    uint8_t version = 0;
    if (!readU8(version) || version != kDescriptorClosureFormatVersion) {
        SC_ASSERT(false, "Unexpected Valdi descriptor closure format version");
        return;
    }

    uint32_t count = 0;
    if (!readU32(count)) {
        SC_ASSERT(false, "Malformed Valdi descriptor closure header");
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        std::string entryClassName;
        uint8_t type = 0;
        ParsedDescriptor descriptor;
        if (!readString(entryClassName) || !readU8(type) || !readString(descriptor.schema) ||
            !readString(descriptor.propertyReplacements) || !readString(descriptor.proxyClassName)) {
            SC_ASSERT(false, "Malformed Valdi descriptor closure entry");
            return;
        }
        descriptor.type = static_cast<int>(type);

        uint32_t refCount = 0;
        if (!readU16(refCount)) {
            SC_ASSERT(false, "Malformed Valdi descriptor closure ref count");
            return;
        }
        descriptor.typeReferenceNames.reserve(refCount);
        for (uint32_t r = 0; r < refCount; r++) {
            std::string refName;
            if (!readString(refName)) {
                SC_ASSERT(false, "Malformed Valdi descriptor closure ref");
                return;
            }
            descriptor.typeReferenceNames.emplace_back(std::move(refName));
        }

        _descriptorByName[Valdi::StringBox::fromString(entryClassName)] = std::move(descriptor);
    }
}

const AndroidValueMarshallerRegistry::ParsedDescriptor* AndroidValueMarshallerRegistry::getOrFetchDescriptorClosure(
    const Valdi::StringBox& className) {
    auto cached = _descriptorByName.find(className);
    if (cached != _descriptorByName.end()) {
        return &cached->second;
    }

    {
        // Disabled in production to keep marshaller-resolution traces lean; uncomment to profile
        // the batched closure fetch locally.
        // VALDI_TRACE_META("Valdi.getDescriptorClosure", className);

        auto jvmClassName = className.replacing('.', '/');
        auto javaClass = JavaClass::resolveOrAbort(JavaEnv(), jvmClassName.getCStr());

        const auto& descriptorJavaClass = ValdiMarshallableObjectDescriptorJavaClass::get();
        // Java tracks which classes it has already packed (its own persistent set) and prunes the walk
        // to the not-yet-resolved frontier, so we don't echo the cache back here. See
        // ValdiMarshallableObjectDescriptor.resolvedClassNames. The lazy-resolution flag is the single
        // source of truth (Valdi::lazyFunctionReturnMarshallerFlag); pass it so the walk skips the
        // lazily-deferred return references without a second Kotlin-side toggle to keep in sync.
        std::initializer_list<JavaValue> parameters = {
            JavaValue::unsafeMakeObject(javaClass.getClass()),
            JavaValue::makeBoolean(Valdi::lazyFunctionReturnMarshallerFlag().load(std::memory_order_relaxed) ? 1 : 0),
        };
        auto bufferValue = descriptorJavaClass.getDescriptorClosureMethod.call(
            reinterpret_cast<jobject>(descriptorJavaClass.cls.getClass()), parameters.size(), parameters.begin());

        jobject bufferObject = bufferValue.getObject();
        if (bufferObject != nullptr) {
            auto* address = static_cast<const uint8_t*>(
                JavaEnv::accessEnvRet([&](JNIEnv& env) -> void* { return env.GetDirectBufferAddress(bufferObject); }));
            auto capacity =
                JavaEnv::accessEnvRet([&](JNIEnv& env) -> jlong { return env.GetDirectBufferCapacity(bufferObject); });
            if (address != nullptr && capacity > 0) {
                parseDescriptorClosure(address, static_cast<size_t>(capacity));
            }
        }
    }

    auto parsed = _descriptorByName.find(className);
    return parsed != _descriptorByName.end() ? &parsed->second : nullptr;
}

Valdi::Result<JavaClassDelegate*> AndroidValueMarshallerRegistry::getOrCreateRegisteredMarshallableClass(
    const Valdi::StringBox& className) {
    auto lock = _schemaRegistry->lock();
    const auto& it = _registeredClassByName.find(className);
    if (it != _registeredClassByName.end()) {
        return dynamic_cast<JavaClassDelegate*>(it->second.get());
    }

    // Disabled in production to avoid per-class trace overhead; resolveValueMarshaller (lower
    // volume) remains as the live guard. Uncomment for local profiling.
    // VALDI_TRACE_META("Valdi.createMarshallableClass", className);

    auto jvmClassName = className.replacing('.', '/');
    auto javaClass = JavaClass::resolveOrAbort(JavaEnv(), jvmClassName.getCStr());

    // Fast path (tweak-gated, default off): resolve the descriptor from the batched closure cache. One
    // getDescriptorClosure JNI call brings in this class plus its transitive references (parsed natively
    // from a ByteBuffer), so reference recursion below becomes cache hits with no further JNI. When the
    // gate is off we skip straight to the legacy per-class path below.
    if (_descriptorClosureEnabled) {
        if (const ParsedDescriptor* cached = getOrFetchDescriptorClosure(className)) {
            // Copy out: a subsequent closure fetch could rehash _descriptorByName and invalidate the ptr.
            ParsedDescriptor descriptor = *cached;
            auto fieldMap = toFieldMap(descriptor.propertyReplacements);
            if (descriptor.type == kTypeInterface) {
                auto proxyJvmName = Valdi::StringBox::fromString(descriptor.proxyClassName).replacing('.', '/');
                auto proxyJavaClass = JavaClass::resolveOrAbort(JavaEnv(), proxyJvmName.getCStr());
                return registerParsedDescriptor(className,
                                                javaClass,
                                                descriptor.type,
                                                descriptor.schema,
                                                fieldMap,
                                                &proxyJavaClass,
                                                descriptor.typeReferenceNames);
            }
            return registerParsedDescriptor(className,
                                            javaClass,
                                            descriptor.type,
                                            descriptor.schema,
                                            fieldMap,
                                            nullptr,
                                            descriptor.typeReferenceNames);
        }
    }

    // Fallback: legacy per-class JNI fetch (getDescriptorForClass + per-field/array read-backs).
    const auto& objectDescriptorJavaClass = ValdiMarshallableObjectDescriptorJavaClass::get();
    std::initializer_list<JavaValue> parameters = {JavaValue::unsafeMakeObject(javaClass.getClass())};

    JavaValue objectDescriptor;
    {
        // Disabled in production to avoid per-class trace overhead; uncomment for local profiling
        // of the legacy per-class descriptor fetch.
        // VALDI_TRACE_META("Valdi.getDescriptorForClass", className);
        objectDescriptor = objectDescriptorJavaClass.getDescriptorForClassMethod.call(
            reinterpret_cast<jobject>(objectDescriptorJavaClass.cls.getClass()), parameters.size(), parameters.begin());
    }

    auto type = objectDescriptorJavaClass.typeField.getFieldValue(objectDescriptor.getObject()).getInt();
    auto schemaSuffix = toStdString(
        JavaEnv(), objectDescriptorJavaClass.schemaField.getFieldValue(objectDescriptor.getObject()).getString());
    auto proxyClass = objectDescriptorJavaClass.proxyClassField.getFieldValue(objectDescriptor.getObject());
    auto typeReferences = objectDescriptorJavaClass.typeReferencesField.getFieldValue(objectDescriptor.getObject());
    auto propertyReplacements =
        objectDescriptorJavaClass.propertyReplacementsField.getFieldValue(objectDescriptor.getObject());

    std::string propertyReplacementsStr;
    if (!propertyReplacements.isNull()) {
        propertyReplacementsStr = toStdString(JavaEnv(), propertyReplacements.getString());
    }
    auto fieldMap = toFieldMap(propertyReplacementsStr);
    auto typeReferenceNames = javaStringArrayToVector(typeReferences);

    if (type == kTypeInterface) {
        auto proxyJavaClass = JavaClass(JavaEnv(), reinterpret_cast<jclass>(proxyClass.getObject()));
        return registerParsedDescriptor(
            className, javaClass, type, schemaSuffix, fieldMap, &proxyJavaClass, typeReferenceNames);
    }
    return registerParsedDescriptor(className, javaClass, type, schemaSuffix, fieldMap, nullptr, typeReferenceNames);
}

} // namespace ValdiAndroid
