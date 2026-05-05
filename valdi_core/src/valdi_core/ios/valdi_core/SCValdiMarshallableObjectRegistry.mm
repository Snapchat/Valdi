//
//  SCValdiMarshallableObjectRegistry.m
//  valdi-ios
//
//  Created by Simon Corsin on 5/29/20.
//

#import "valdi_core/SCValdiMarshallableObjectRegistry.h"
#import "valdi_core/SCValdiBuiltinTrampolines.h"
#import "valdi_core/SCValdiMarshallableObjectUtils.h"
#import "valdi_core/SCValdiMarshallableObject.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiMarshallable.h"
#import "valdi_core/SCValdiMarshaller+CPP.h"

#import "valdi_core/SCValdiObjCValue.h"
#import "valdi_core/SCValdiObjCValueDelegate.h"
#import "valdi_core/SCValdiBlockHelper.h"

#import "valdi_core/SCValdiMarshaller+CPP.h"

#import "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#import "valdi_core/cpp/Utils/ValueMarshallerRegistry.hpp"
#import "valdi_core/cpp/Constants.hpp"
#import "valdi_core/cpp/Utils/PlatformValueDelegate.hpp"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiValueUtils.h"
#import "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#import "valdi_core/cpp/Threading/DispatchQueue.hpp"

#import "valdi_core/SCNValdiCoreAsset.h"

#import <objc/runtime.h>

namespace ValdiIOS {

template<typename T>
static void checkResult(const Valdi::Result<T> &result) {
    if (!result) {
        NSExceptionThrowFromError(result.error());
    }
}

static void ensureNotOptional(const Valdi::ValueSchema &schema) {
    SC_ASSERT(!schema.isOptional(), "Primitive types can only be optional if they are boxed");
}

static SCValdiFieldValueType SCValdiGetFieldValueTypeForTypeReference(const Valdi::ValueSchemaTypeReference &typeReference, bool isBoxed) {
    if (!isBoxed && typeReference.getTypeHint() == Valdi::ValueSchemaTypeReferenceTypeHintEnum) {
        return SCValdiFieldValueTypeInt;
    } else {
        return SCValdiFieldValueTypeObject;
    }
}

static SCValdiFieldValueDescriptor SCValdiFieldValueDescriptorFromSchema(const Valdi::ValueSchema &schema) {
    if (schema.isSchemaReference()) {
        return SCValdiFieldValueDescriptorFromSchema(schema.getReferencedSchema());
    }

    SCValdiFieldValueDescriptor descriptor = SCValdiFieldValueDescriptorMakeEmpty();;
    descriptor.isOptional = schema.isOptional();

    if (schema.isUntyped()) {
        descriptor.type = SCValdiFieldValueTypeObject;
    } else if (schema.isString()) {
        descriptor.type = SCValdiFieldValueTypeObject;
        descriptor.isCopyable = YES;
    } else if (schema.isInteger()) {
        if (schema.isBoxed()) {
            descriptor.type = SCValdiFieldValueTypeObject;
        } else {
            descriptor.type = SCValdiFieldValueTypeInt;
            ensureNotOptional(schema);
        }
    } else if (schema.isLongInteger()) {
        if (schema.isBoxed()) {
            descriptor.type = SCValdiFieldValueTypeObject;
        } else {
            descriptor.type = SCValdiFieldValueTypeLong;
            ensureNotOptional(schema);
        }
    } else if (schema.isDouble()) {
        if (schema.isBoxed()) {
            descriptor.type = SCValdiFieldValueTypeObject;
        } else {
            descriptor.type = SCValdiFieldValueTypeDouble;
            ensureNotOptional(schema);
        }
    } else if (schema.isBoolean()) {
        if (schema.isBoxed()) {
            descriptor.type = SCValdiFieldValueTypeObject;
        } else {
            descriptor.type = SCValdiFieldValueTypeBool;
            ensureNotOptional(schema);
        }
    } else if (schema.isFunction()) {
        descriptor.type = SCValdiFieldValueTypeObject;
        descriptor.isCopyable = YES;
        descriptor.isFunction = YES;
    } else if (schema.isValueTypedArray()) {
        descriptor.type = SCValdiFieldValueTypeObject;
        descriptor.isCopyable = YES;
    } else if (schema.isArray()) {
        descriptor.type = SCValdiFieldValueTypeObject;
        descriptor.isCopyable = YES;
    } else if (schema.isTypeReference()) {
        descriptor.type = SCValdiGetFieldValueTypeForTypeReference(schema.getTypeReference(), schema.isBoxed());
    } else if (schema.isGenericTypeReference()) {
        const auto *genericTypeReference = schema.getGenericTypeReference();
        descriptor.type = SCValdiGetFieldValueTypeForTypeReference(genericTypeReference->getType(), schema.isBoxed());
    } else if (schema.isEnum()) {
        const auto *enumSchema = schema.getEnum();
        if (schema.isBoxed() || enumSchema->getCaseSchema().isString()) {
            descriptor.type = SCValdiFieldValueTypeObject;
        } else {
            descriptor.type = SCValdiFieldValueTypeInt;
        }
    } else if (schema.isClass()) {
        descriptor.type = SCValdiFieldValueTypeObject;
    } else if (schema.isMap()) {
        descriptor.type = SCValdiFieldValueTypeObject;
        descriptor.isCopyable = YES;
    } else if (schema.isPromise()) {
        descriptor.type = SCValdiFieldValueTypeObject;
    } else {
        std::abort();
    }

    return descriptor;
}

static Valdi::Result<Valdi::ValueSchema> schemaFromObjectDescriptor(std::string_view className,
                                                                          const SCValdiMarshallableObjectDescriptor &objectDescriptor) {
    Valdi::SmallVector<std::pair<std::string, std::string>, 8> replacementPairs;
    // Replace the identifiers inside the schema string.
    // This is used mostly to make the schema string description shorter
    // and use slightly less app binary size.
    const auto *const *identifiers = objectDescriptor.identifiers;
    if (identifiers != nullptr) {
        size_t identifierIndex = 0;
        while (*identifiers != nullptr) {
            replacementPairs.emplace_back(std::make_pair(fmt::format("[{}]", identifierIndex), std::string(std::string_view(*identifiers))));

            identifiers++;
            identifierIndex++;
        }
    }

    Valdi::SmallVector<Valdi::ClassPropertySchema, 16> properties;

    // Build the class schema string representation using the given fields
    const auto *fieldDescriptors = objectDescriptor.fields;
    if (fieldDescriptors != nil) {
        while (fieldDescriptors->name) {
            const char *name = fieldDescriptors->name;
            const char *type = fieldDescriptors->type;
            auto propertyName = Valdi::StringCache::getGlobal().makeStringFromCLiteral(name);
            auto typeCpp = std::string(std::string_view(type));
            for (const auto &it: replacementPairs) {
                Valdi::stringReplace(typeCpp, it.first, it.second);
            }

            auto propertySchema = Valdi::ValueSchema::parse(typeCpp);
            if (!propertySchema) {
                return propertySchema.error().rethrow(STRING_FORMAT("While parsing property '{}' of {}", propertyName, className));
            }

            properties.emplace_back(Valdi::ClassPropertySchema(propertyName, propertySchema.value()));

            fieldDescriptors++;
        }
    }

    auto cppClassName = Valdi::StringCache::getGlobal().makeString(className);

    switch (objectDescriptor.objectType) {
        case SCValdiMarshallableObjectTypeClass:
            return Valdi::ValueSchema::cls(cppClassName, false, properties.data(), properties.size());
        case SCValdiMarshallableObjectTypeProtocol:
            return Valdi::ValueSchema::cls(cppClassName, true, properties.data(), properties.size());
        case SCValdiMarshallableObjectTypeFunction:
            return Valdi::ValueSchema::cls(cppClassName, false, properties.data(), properties.size());
        case SCValdiMarshallableObjectTypeUntyped:
            return Valdi::ValueSchema::untyped();
    }
}

static Valdi::StringBox enumClassFromEnum(id<SCValdiEnum> valdiEnum) {
    Class cls = [valdiEnum class];
    NSMutableString *str = [NSStringFromClass(cls) mutableCopy];
    NSString *enumSuffix = VALDI_ENUM_CLASS_SUFFIX;
    [str deleteCharactersInRange:NSMakeRange(str.length - enumSuffix.length, enumSuffix.length)];
    return ValdiIOS::StringFromNSString(str);
}

static Valdi::ValueSchema schemaFromStringEnum(SCValdiStringEnum *stringEnum, const Valdi::StringBox &enumClassName) {
    NSUInteger caseCount = stringEnum.count;
    Valdi::SmallVector<Valdi::EnumCaseSchema, 16> cases;
    cases.reserve(caseCount);

    for (NSUInteger i = 0; i < caseCount; i++) {
        NSString *enumCase = [stringEnum enumCaseForIndex:i];
        cases.emplace_back(STRING_FORMAT("{}", i), Valdi::Value(ValdiIOS::StringFromNSString(enumCase)));
    }

    return Valdi::ValueSchema::enumeration(enumClassName, Valdi::ValueSchema::string(), cases.data(), cases.size());
}

static Valdi::ValueSchema schemaFromIntEnum(SCValdiIntEnum *intEnum, const Valdi::StringBox &enumClassName) {
    NSUInteger caseCount = intEnum.count;
    Valdi::SmallVector<Valdi::EnumCaseSchema, 16> cases;
    cases.reserve(caseCount);

    for (NSUInteger i = 0; i < caseCount; i++) {
        NSInteger enumCase = [intEnum enumCaseForIndex:i];
        cases.emplace_back(STRING_FORMAT("{}", i), Valdi::Value(static_cast<int32_t>(enumCase)));
    }

    return Valdi::ValueSchema::enumeration(enumClassName, Valdi::ValueSchema::integer(), cases.data(), cases.size());
}

static Valdi::StringBox SCValdiBlockSignatureFromFunctionSchema(const Valdi::ValueFunctionSchema &schema) {
    std::string blockSignature;

    if (schema.getAttributes().isMethod()) {
        // Objective-C functions always start two pointers
        blockSignature.append(2, SCValdiCTypeCharForFieldType(SCValdiFieldValueTypeObject));
    } else {
        // Block start with one pointer
        blockSignature.append(1, SCValdiCTypeCharForFieldType(SCValdiFieldValueTypeObject));
    }

    for (size_t i = 0; i < schema.getParametersSize(); i++) {
        const auto &parameter = schema.getParameter(i);
        auto fieldDescriptor = SCValdiFieldValueDescriptorFromSchema(parameter);
        blockSignature.append(1, SCValdiCTypeCharForFieldType(fieldDescriptor.type));
    }

    blockSignature.append(":");

    if (schema.getReturnValue().isVoid()) {
        blockSignature.append("v");
    } else {
        auto fieldDescriptor = SCValdiFieldValueDescriptorFromSchema(schema.getReturnValue());
        blockSignature.append(1, SCValdiCTypeCharForFieldType(fieldDescriptor.type));
    }

    return Valdi::StringCache::getGlobal().makeString(std::move(blockSignature));
}

class ObjCClassDelegate: public Valdi::PlatformObjectClassDelegate<ObjCValue> {
public:
    ~ObjCClassDelegate() override = default;

    Valdi::Ref<Valdi::ValueTypedProxyObject> newProxy(const ObjCValue& object,
                                                            const Valdi::Ref<Valdi::ValueTypedObject> &typedObject,
                                                            Valdi::ExceptionTracker &exceptionTracker) final {
        id<SCValdiMarshallable> marshallable = object.getObject();
        BOOL strongRef = YES;
        if ([marshallable respondsToSelector:@selector(shouldRetainInstanceWhenMarshalling)]) {
            strongRef = [marshallable shouldRetainInstanceWhenMarshalling];
        }
        return ValdiIOS::ProxyObjectFromNSObject(marshallable, typedObject, strongRef);
    }

    Valdi::ValueSchemaRegistrySchemaIdentifier getIdentifier() const {
        return _identifier;
    }

    Valdi::ValueMarshaller<ObjCValue> *getValueMarshaller() const {
        return _valueMarshaller;
    }

    void setValueMarshaller(Valdi::ValueMarshaller<ObjCValue> *valueMarshaller) {
        _valueMarshaller = valueMarshaller;
    }

    Valdi::ClassSchema *getSchema() const {
        return _schema;
    }

    void setSchema(Valdi::ClassSchema *schema) {
        _schema = schema;
    }

    size_t getFieldsCount() const {
        return _fieldsCount;
    }

    const SCValdiFieldValueDescriptor *getFieldDescriptors() const {
        Valdi::InlineContainerAllocator<ObjCClassDelegate, SCValdiFieldValueDescriptor> allocator;
        return allocator.getContainerStartPtr(this);
    }

    const SCValdiFieldValueDescriptor &getFieldDescriptor(size_t index) const {
        return getFieldDescriptors()[index];
    }

    SCValdiFieldValueDescriptor &getFieldDescriptor(size_t index) {
        Valdi::InlineContainerAllocator<ObjCClassDelegate, SCValdiFieldValueDescriptor> allocator;
        return allocator.getContainerStartPtr(this)[index];
    }

    SCValdiFieldValue *allocateFieldsStorage() const {
        return SCValdiAllocateFieldsStorage(_fieldsCount);
    }

    SCValdiFieldValue *allocateFieldsStorageWithValues(va_list values) const {
        SCValdiFieldValue *storage = allocateFieldsStorage();
        SCValdiSetFieldsStorageWithValuesList(storage, _fieldsCount, getFieldDescriptors(), values);
        return storage;
    }

    void deallocateFieldsStorage(SCValdiFieldValue *fieldsStorage) const {
        SCValdiDeallocateFieldsStorage(fieldsStorage, _fieldsCount, getFieldDescriptors());
    }

protected:
    Class _cls;
    __unsafe_unretained id _objectRegistry;
    Valdi::ValueSchemaRegistrySchemaIdentifier _identifier;
    Valdi::ValueMarshaller<ObjCValue> *_valueMarshaller = nullptr;
    Valdi::ClassSchema *_schema = nullptr;
    size_t _fieldsCount;
    Valdi::StringBox _callBlockPropertyName; // Used for ObjcClassFunction

    friend Valdi::InlineContainerAllocator<ObjCClassDelegate, SCValdiFieldValueDescriptor>;

    ObjCClassDelegate(Class cls,
              __unsafe_unretained id objectRegistry,
              Valdi::ValueSchemaRegistrySchemaIdentifier identifier,
              size_t fieldsCount): _cls(cls), _objectRegistry(objectRegistry), _identifier(identifier), _fieldsCount(fieldsCount) {
        Valdi::InlineContainerAllocator<ObjCClassDelegate, SCValdiFieldValueDescriptor> allocator;
        allocator.constructContainerEntries(this, _fieldsCount);
    }
};

class ObjCClassProtocolDelegate: public ObjCClassDelegate {
public:
    ~ObjCClassProtocolDelegate() final = default;

    ObjCValue newObject(const ObjCValue* propertyValues, Valdi::ExceptionTracker &exceptionTracker) final {
        SCValdiFieldValue *storage = allocateFieldsStorage();

        ObjCValue::toFieldValues(_fieldsCount, propertyValues, getFieldDescriptors(), [&](size_t fieldsCount, const SCValdiFieldValueDescriptor *fieldDescriptors, const SCValdiFieldValue *fieldValues) {
            SCValdiSetFieldsStorageWithValues(storage, fieldsCount, fieldDescriptors, fieldValues);
        });

        SCValdiProxyMarshallableObject *proxy = [[_cls alloc] initWithObjectRegistry:_objectRegistry storage:storage];

        return ObjCValue::makeObject(proxy);
    }

    ObjCValue getProperty(const ObjCValue& object, size_t propertyIndex, Valdi::ExceptionTracker &exceptionTracker) final {
        const auto &fieldDescriptor = getFieldDescriptor(propertyIndex);

        id instance = object.getObject();
        if ([instance isKindOfClass:_cls]) {
            // This is a bridged class, return the field value
            return ObjCValue(SCValdiGetMarshallableObjectFieldsStorage(instance)[propertyIndex], fieldDescriptor.type);
        }

        if (fieldDescriptor.isFunction) {
            // For Objective-C methods we return the selector
            if (fieldDescriptor.isOptional && ![instance respondsToSelector:fieldDescriptor.selector]) {
                return ObjCValue::makeNull();
            }
            return ObjCValue::makePtr(fieldDescriptor.selector);
        } else {
            return ObjCValue(SCValdiGetFieldValueFromBridgedObject(instance, fieldDescriptor), fieldDescriptor.type);
        }
    }

    static Valdi::Ref<ObjCClassProtocolDelegate> make(__unsafe_unretained Class cls,
                                         __unsafe_unretained id objectRegistry,
                                         Valdi::ValueSchemaRegistrySchemaIdentifier identifier,
                                         size_t fieldsCount) {
        Valdi::InlineContainerAllocator<ObjCClassProtocolDelegate, SCValdiFieldValueDescriptor> allocator;
        return allocator.allocate(fieldsCount, cls, objectRegistry, identifier, fieldsCount);
    }

private:
    friend Valdi::InlineContainerAllocator<ObjCClassProtocolDelegate, SCValdiFieldValueDescriptor>;

    ObjCClassProtocolDelegate(Class cls,
                      __unsafe_unretained id objectRegistry,
                      Valdi::ValueSchemaRegistrySchemaIdentifier identifier,
                      size_t fieldsCount): ObjCClassDelegate(cls, objectRegistry, identifier, fieldsCount) {}
};

class ObjCClassModelDelegate: public ObjCClassDelegate {
public:
    ~ObjCClassModelDelegate() override = default;

    ObjCValue newObject(const ObjCValue* propertyValues, Valdi::ExceptionTracker &exceptionTracker) final {
        SCValdiFieldValue *storage = allocateFieldsStorage();

        ObjCValue::toFieldValues(_fieldsCount, propertyValues, getFieldDescriptors(), [&](size_t fieldsCount, const SCValdiFieldValueDescriptor *fieldDescriptors, const SCValdiFieldValue *fieldValues) {
            SCValdiSetFieldsStorageWithValues(storage, fieldsCount, fieldDescriptors, fieldValues);
        });

        return ObjCValue::makeObject([[_cls alloc] initWithObjectRegistry:_objectRegistry storage:storage]);
    }

    ObjCValue getProperty(const ObjCValue& object, size_t propertyIndex, Valdi::ExceptionTracker &exceptionTracker) final {
        const auto &fieldDescriptor = getFieldDescriptor(propertyIndex);
        __unsafe_unretained id nsObject = object.getObject();
        if (!nsObject) {
            if constexpr (kStrictNullCheckingEnabled) {
                exceptionTracker.onError("Object is null");
            }

            return ObjCValue(SCValdiFieldValueMakeNull(), fieldDescriptor.type);
        }

        return ObjCValue(SCValdiGetMarshallableObjectFieldsStorage(nsObject)[propertyIndex], fieldDescriptor.type);
    }

    static Valdi::Ref<ObjCClassModelDelegate> make(__unsafe_unretained Class cls,
                                         __unsafe_unretained id objectRegistry,
                                         Valdi::ValueSchemaRegistrySchemaIdentifier identifier,
                                         size_t fieldsCount) {
        Valdi::InlineContainerAllocator<ObjCClassModelDelegate, SCValdiFieldValueDescriptor> allocator;
        return allocator.allocate(fieldsCount, cls, objectRegistry, identifier, fieldsCount);
    }

protected:
    friend Valdi::InlineContainerAllocator<ObjCClassModelDelegate, SCValdiFieldValueDescriptor>;

    ObjCClassModelDelegate(Class cls,
                           __unsafe_unretained id objectRegistry,
                           Valdi::ValueSchemaRegistrySchemaIdentifier identifier,
                           size_t fieldsCount): ObjCClassDelegate(cls, objectRegistry, identifier, fieldsCount) {}
};

class ObjCStringEnumClassDelegate: public Valdi::PlatformEnumClassDelegate<ObjCValue> {
public:
    ObjCStringEnumClassDelegate(SCValdiStringEnum *objcEnum): _objcEnum(objcEnum) {}

    ~ObjCStringEnumClassDelegate() override = default;

    ObjCValue newEnum(size_t enumCaseIndex, bool /*isBoxed*/, Valdi::ExceptionTracker &exceptionTracker) final {
        NSString *enumCase = [_objcEnum enumCaseForIndex:enumCaseIndex];
        return ObjCValue::makeObject(enumCase);
    }

    Valdi::Value enumCaseToValue(const ObjCValue& enumeration, bool /*isBoxed*/, Valdi::ExceptionTracker &exceptionTracker) final {
        return Valdi::Value(ValdiIOS::StringFromNSString(enumeration.getObject()));
    }

private:
    SCValdiStringEnum *_objcEnum;
};

class ObjcIntEnumClassDelegate: public Valdi::PlatformEnumClassDelegate<ObjCValue> {
public:
    ObjcIntEnumClassDelegate(SCValdiIntEnum *objcEnum): _objcEnum(objcEnum) {}

    ~ObjcIntEnumClassDelegate() override = default;

    ObjCValue newEnum(size_t enumCaseIndex, bool isBoxed, Valdi::ExceptionTracker &exceptionTracker) final {
        NSInteger enumCase = [_objcEnum enumCaseForIndex:enumCaseIndex];
        if (isBoxed) {
            return ObjCValue::makeObject(@(enumCase));
        } else {
            return ObjCValue::makeInt(static_cast<int32_t>(enumCase));
        }
    }

    Valdi::Value enumCaseToValue(const ObjCValue& enumeration, bool isBoxed, Valdi::ExceptionTracker &exceptionTracker) final {
        if (isBoxed) {
            int intValue = [(NSNumber *)enumeration.getObject() intValue];
            return Valdi::Value(static_cast<int32_t>(intValue));
        } else {
            return Valdi::Value(static_cast<int32_t>(enumeration.getInt()));
        }
    }

private:
    SCValdiIntEnum *_objcEnum;
};

static Valdi::StringBox StringFromClass(__unsafe_unretained Class cls) {
    return ValdiIOS::StringFromNSString(NSStringFromClass(cls));
}

class ObjCMarshallableObjectRegistry;

class ValueSchemaRegistryListenerImpl : public Valdi::ValueSchemaRegistryListener {
public:
    ValueSchemaRegistryListenerImpl(ObjCMarshallableObjectRegistry &registry);

    Valdi::Result<Valdi::Void> resolveSchemaIdentifierForSchemaKey(Valdi::ValueSchemaRegistry& registry,
                                                                                                        const Valdi::ValueSchemaRegistryKey& registryKey) final;

private:
    ObjCMarshallableObjectRegistry &_registry;
};

class ObjCMarshallableObjectRegistry: public Valdi::SimpleRefCountable, public ObjCTypesResolver {
public:
    ObjCMarshallableObjectRegistry(__unsafe_unretained id objcObjectRegistry):
    _objcObjectRegistry(objcObjectRegistry),
    _schemaRegistry(Valdi::makeShared<Valdi::ValueSchemaRegistry>()),
    _valueMarshallerRegistry(Valdi::ValueSchemaTypeResolver(_schemaRegistry.get()),
                             Valdi::makeShared<ObjCValueDelegate>(*this),
                             Valdi::DispatchQueue::create(STRING_LITERAL("Valdi Callback Queue"), Valdi::ThreadQoSClassHigh),
                             STRING_LITERAL("ObjC")) {
        _schemaRegistry->setListener(Valdi::makeShared<ValueSchemaRegistryListenerImpl>(*this));
    }

    ~ObjCMarshallableObjectRegistry() override = default;

    std::unique_lock<std::recursive_mutex> lock() const {
        return _schemaRegistry->lock();
    }

    Valdi::ValueSchemaRegistry* getValueSchemaRegistryPtr() {
        return _schemaRegistry.get();
    }

    void registerUntyped(NSString *typeName) {
        auto guard = lock();

        auto schemaKey = Valdi::ValueSchema::typeReference(Valdi::ValueSchemaTypeReference::named(ValdiIOS::StringFromNSString(typeName)));
        _schemaRegistry->registerSchema(schemaKey, Valdi::ValueSchema::untyped());
    }

    Valdi::Result<Valdi::ValueSchemaRegistrySchemaIdentifier> registerClass(Class objectClass, SCValdiMarshallableObjectDescriptor objectDescriptor) {
        auto guard = lock();

        auto cppClassName = StringFromClass(objectClass);
        auto result = ValdiIOS::schemaFromObjectDescriptor(cppClassName.toStringView(), objectDescriptor);
        if (!result) {
            return result.moveError();
        }
        if (result.value().isUntyped()) {
            return _schemaRegistry->registerSchema(Valdi::ValueSchema::typeReference(Valdi::ValueSchemaTypeReference::named(cppClassName)),
                                                   result.value());
        }

        auto isInterface = objectDescriptor.objectType == SCValdiMarshallableObjectTypeProtocol;

        if (isInterface) {
            // Add Protocol conformance of the Proxy class.
            // We can generate this compile time but this result in larger binary size impact.
            // This makes it possible to cast a proxy instance to a protocol in Swift.
            Protocol *protocol = NSProtocolFromString(NSStringFromClass(objectClass));
            if (protocol) {
                class_addProtocol(objectClass, protocol);
            }
        } else if (![objectClass isSubclassOfClass:[SCValdiMarshallableObject class]]) {
            return Valdi::Error(STRING_FORMAT("Class '{}' does not inherit '{}'", StringFromClass(objectClass), StringFromClass([SCValdiMarshallableObject class])));
        }

        auto classRef = result.value().getClassRef();

        auto identifier = _schemaRegistry->registerSchema(result.value());

        auto classDelelegate = makeClassDelegate(objectClass,
                                                 identifier,
                                                 objectDescriptor.objectType,
                                                 classRef);
        registerBlockHelpers(objectDescriptor.blocks);

        for (size_t i = 0; i < classRef->getPropertiesSize(); i++) {
            const auto &property = classRef->getProperty(i);
            const auto &fieldDescriptor = objectDescriptor.fields[i];
            auto fieldValueDescriptor = ValdiIOS::SCValdiFieldValueDescriptorFromSchema(property.schema);

            const char *selName = fieldDescriptor.selName;
            if (!selName) {
                selName = property.name.getCStr();
            }

            fieldValueDescriptor.selector = sel_registerName(selName);

            classDelelegate->getFieldDescriptor(i) = fieldValueDescriptor;

            switch (objectDescriptor.objectType) {
                case SCValdiMarshallableObjectTypeClass:
                    SCValdiAppendFieldGetter(fieldValueDescriptor, i, objectClass);
                    SCValdiAppendFieldSetter(fieldValueDescriptor, selName, i, objectClass);
                    break;
                case SCValdiMarshallableObjectTypeProtocol: {
                    if (property.schema.isFunction()) {
                        const auto &function = *property.schema.getFunction();
                        auto blockHelper = getBlockHelperForSchema(function);
                        if (blockHelper == nullptr) {
                            return Valdi::Error(STRING_FORMAT("Could not resolve block helper for type signature {}", property.schema.toString()));
                        }
                        auto proxy = blockHelper->makeSelectorImpProxy(i);
                        IMP imp = imp_implementationWithBlock(proxy.block.getObject());

                        class_addMethod(objectClass, fieldValueDescriptor.selector, imp, proxy.objcTypeEncoding.c_str());
                    } else {
                        SCValdiAppendFieldGetter(fieldValueDescriptor, i, objectClass);
                    }
                }
                    break;
                case SCValdiMarshallableObjectTypeFunction:
                case SCValdiMarshallableObjectTypeUntyped:
                    // No need to generate methods for untyped and exported functions.
                    break;
            }
        }

        _objectClassDelegateByIdentifier[identifier] = classDelelegate;

        _schemaIdentifierByClass.try_emplace((__bridge const void *)objectClass, identifier);

        return identifier;
    }

    Valdi::ValueSchemaRegistrySchemaIdentifier doRegisterEnum(Class objectClass,
                                                                 const Valdi::StringBox &enumName,
                                                                 const Valdi::ValueSchema &schema,
                                                                 const Valdi::Ref<Valdi::PlatformEnumClassDelegate<ObjCValue>> &enumClassDelegate,
                                                                 bool isIntEnum) {
        auto guard = lock();

        auto schemaKey = Valdi::ValueSchema::typeReference(Valdi::ValueSchemaTypeReference::named(enumName));
        auto identifier = _schemaRegistry->registerSchema(schemaKey, schema);
        _enumClassDelegateByIdentifier[identifier] = enumClassDelegate;
        _schemaIdentifierByClass.try_emplace((__bridge const void *)objectClass, identifier);

        if (isIntEnum) {
            // Also register the boxed variants
            _schemaRegistry->registerSchema(schemaKey.asBoxed(), schema.asBoxed());
        }

        return identifier;
    }

    Valdi::Result<Valdi::ValueSchemaRegistrySchemaIdentifier> registerEnum(id<SCValdiEnum> valdiEnum) {
        auto enumClassName = enumClassFromEnum(valdiEnum);

        if ([valdiEnum isKindOfClass:[SCValdiStringEnum class]]) {
            auto schema = schemaFromStringEnum(valdiEnum, enumClassName);
            auto enumClassDelegate = Valdi::makeShared<ObjCStringEnumClassDelegate>(valdiEnum);

            return doRegisterEnum([valdiEnum class], enumClassName, schema, enumClassDelegate, false);
        } else if ([valdiEnum isKindOfClass:[SCValdiIntEnum class]]) {
            auto schema = schemaFromIntEnum(valdiEnum, enumClassName);
            auto enumClassDelegate = Valdi::makeShared<ObjcIntEnumClassDelegate>(valdiEnum);

            return doRegisterEnum([valdiEnum class], enumClassName, schema, enumClassDelegate, true);
        } else {
            return Valdi::Error("Unrecognized enum type");
        }
    }

    size_t registerBlockHelpers(const SCValdiMarshallableObjectBlockSupport *blocks) {
        size_t totalRegistered = 0;

        if (blocks != nullptr) {
            while (blocks->typeEncoding != nullptr) {
                if (registerBlockHelper(blocks->typeEncoding, blocks->invoker, blocks->factory)) {
                    totalRegistered++;
                }
                blocks++;
            }
        }

        return totalRegistered;
    }

    bool registerBlockHelper(const char *typeEncoding, SCValdiFunctionInvoker invoker, SCValdiBlockFactory factory) {
        return registerBlockHelper(Valdi::StringCache::getGlobal().makeStringFromCLiteral(typeEncoding), invoker, factory);
    }

    bool registerBlockHelper(const Valdi::StringBox &typeEncoding, SCValdiFunctionInvoker invoker, SCValdiBlockFactory factory) {
        auto guard = lock();
        const auto &it = _blockHelpers.find(typeEncoding);
        if (it != _blockHelpers.end())  {
            return false;
        }

        _blockHelpers[typeEncoding] = ObjCBlockHelper::make(typeEncoding, invoker, factory);

        return true;
    }

    Valdi::Result<Valdi::Ref<ObjCClassDelegate>> getOrCreateObjectClassDelegateForNSClass(__unsafe_unretained Class cls) {
        auto guard = lock();

        if (cls == nil) {
            return Valdi::Error("Cannot create object class delegate for nil class. This usually means a handler or payload class was not properly registered.");
        }

        auto classDelegate = getObjectClassDelegateForNSClass(cls);
        if (classDelegate != nullptr) {
            return classDelegate;
        }

        if (![cls respondsToSelector:@selector(valdiMarshallableObjectDescriptor)]) {
            return Valdi::Error(STRING_FORMAT("Cannot automatically create object class for '{}' as it does not implement the valdiMarshallableObjectDescriptor class method", StringFromClass(cls)));
        }

        auto result = registerClass(cls, [cls valdiMarshallableObjectDescriptor]);
        if (!result) {
            return result.moveError();
        }

        classDelegate = getObjectClassDelegateForNSClass(cls);

        if (classDelegate == nullptr) {
            return Valdi::Error(STRING_FORMAT("Class {} was declared as marshallable but got registered as untyped", StringFromClass(cls)));
        }

        return classDelegate;
    }

    Valdi::Result<int> marshallObjectOfClass(__unsafe_unretained Class cls, __unsafe_unretained id instance, Valdi::Marshaller &marshaller) {
        auto guard = lock();
        auto classDelegate = getLoadedObjectClassDelegateForNSClass(cls);
        if (!classDelegate) {
            return classDelegate.moveError();
        }

        guard.unlock();

        Valdi::SimpleExceptionTracker exceptionTracker;
        auto object = classDelegate.value()->getValueMarshaller()->marshall(nullptr,
                                                                            ObjCValue::makeUnretainedObject(instance),
                                                                            Valdi::ReferenceInfoBuilder(),
                                                                            exceptionTracker);

        if (!exceptionTracker) {
            return exceptionTracker.extractError();
        }

        return marshaller.push(object);
    }

    Valdi::Result<Valdi::Void> setActiveSchemaOfClassInMarshaller(__unsafe_unretained Class cls,
                                                                        Valdi::Marshaller &marshaller) {
        auto guard = lock();
        auto classDelegate = getLoadedObjectClassDelegateForNSClass(cls);
        if (!classDelegate) {
            return classDelegate.moveError();
        }

        guard.unlock();

        auto schema = classDelegate.value()->getSchema();
        SC_ASSERT(schema != nullptr);
        marshaller.setCurrentSchema(Valdi::ValueSchema::cls(Valdi::strongSmallRef(schema)));

        return Valdi::Void();
    }

    Valdi::Result<Valdi::Void> forceLoadObjectClass(__unsafe_unretained Class cls) {
        auto guard = lock();
        auto classDelegate = getLoadedObjectClassDelegateForNSClass(cls);
        if (!classDelegate) {
            return classDelegate.moveError();
        }

        return Valdi::Void();
    }

    Valdi::Result<ObjCValue> unmarshallObjectOfClass(__unsafe_unretained Class cls, Valdi::Marshaller &marshaller, int objectIndex) {
        auto guard = lock();
        auto classDelegate = getLoadedObjectClassDelegateForNSClass(cls);
        if (!classDelegate) {
            return classDelegate.moveError();
        }

        guard.unlock();

        Valdi::SimpleExceptionTracker exceptionTracker;
        auto result = classDelegate.value()->getValueMarshaller()->unmarshall(marshaller.getOrUndefined(objectIndex), Valdi::ReferenceInfoBuilder(), exceptionTracker);

        if (!exceptionTracker) {
            return exceptionTracker.extractError();
        }

        return std::move(result);
    }

    Valdi::Ref<ObjCClassDelegate> getObjectClassDelegateForNSClass(__unsafe_unretained Class cls) const {
        auto guard = lock();
        const auto &it = _schemaIdentifierByClass.find((__bridge  const void *)cls);
        if (it == _schemaIdentifierByClass.end()) {
            return nullptr;
        }

        return _objectClassDelegateByIdentifier.find(it->second)->second;
    }

    Valdi::Ref<Valdi::PlatformEnumClassDelegate<ObjCValue>> getEnumClassDelegateForNSClass(__unsafe_unretained Class cls) const {
        auto guard = lock();
        const auto &it = _schemaIdentifierByClass.find((__bridge  const void *)cls);
        if (it == _schemaIdentifierByClass.end()) {
            return nullptr;
        }

        return _enumClassDelegateByIdentifier.find(it->second)->second;
    }

    Valdi::Ref<Valdi::PlatformObjectClassDelegate<ObjCValue>> getObjectClassDelegateForName(const Valdi::StringBox& className) const final {
        auto cls = NSClassFromString(ValdiIOS::NSStringFromString(className));
        SC_ASSERT(cls != nullptr);

        return getObjectClassDelegateForNSClass(cls);
    }

    Valdi::Ref<Valdi::PlatformEnumClassDelegate<ObjCValue>> getEnumClassDelegateForName(const Valdi::StringBox& enumName) const final {
        NSString *enumClassName = ValdiIOS::NSStringFromString(enumName);
        NSString *resolvedEnumClassName = [enumClassName stringByAppendingString:VALDI_ENUM_CLASS_SUFFIX];
        auto cls = NSClassFromString(resolvedEnumClassName);
        SC_ASSERT(cls != nullptr);

        return getEnumClassDelegateForNSClass(cls);
    }

    Valdi::Ref<ObjCBlockHelper> getBlockHelperForSchema(const Valdi::ValueFunctionSchema &schema) const final {
        auto typeEncoding = SCValdiBlockSignatureFromFunctionSchema(schema);
        const auto &it = _blockHelpers.find(typeEncoding);
        if (it == _blockHelpers.end()) {
            return nullptr;
        }

        return it->second;
    }

private:
    __unsafe_unretained id _objcObjectRegistry;
    Valdi::Ref<Valdi::ValueSchemaRegistry> _schemaRegistry;
    Valdi::ValueMarshallerRegistry<ObjCValue> _valueMarshallerRegistry;
    Valdi::FlatMap<Valdi::StringBox, Valdi::Ref<ObjCBlockHelper>> _blockHelpers;
    Valdi::FlatMap<Valdi::ValueSchemaRegistrySchemaIdentifier, Valdi::Ref<ValdiIOS::ObjCClassDelegate>> _objectClassDelegateByIdentifier;
    Valdi::FlatMap<Valdi::ValueSchemaRegistrySchemaIdentifier, Valdi::Ref<Valdi::PlatformEnumClassDelegate<ValdiIOS::ObjCValue>>> _enumClassDelegateByIdentifier;
    Valdi::FlatMap<const void *, Valdi::ValueSchemaRegistrySchemaIdentifier> _schemaIdentifierByClass;

    Valdi::Ref<ObjCClassDelegate> makeClassDelegate(__unsafe_unretained Class objectClass,
                                               Valdi::ValueSchemaRegistrySchemaIdentifier identifier,
                                               SCValdiMarshallableObjectType objectType,
                                               const Valdi::Ref<Valdi::ClassSchema> &classSchema) {
        auto propertiesSize = classSchema->getPropertiesSize();
        switch (objectType) {
            case SCValdiMarshallableObjectTypeClass:
                return ObjCClassModelDelegate::make(objectClass, _objcObjectRegistry, identifier, propertiesSize);
            case SCValdiMarshallableObjectTypeProtocol:
                return ObjCClassProtocolDelegate::make(objectClass, _objcObjectRegistry, identifier, propertiesSize);
            case SCValdiMarshallableObjectTypeFunction:
                return ObjCClassModelDelegate::make(objectClass, _objcObjectRegistry, identifier, propertiesSize);
            case SCValdiMarshallableObjectTypeUntyped:
                std::abort();
        }
    }

    Valdi::Result<Valdi::Ref<ObjCClassDelegate>> getLoadedObjectClassDelegateForNSClass(__unsafe_unretained Class cls) {
        auto classDelegate = getOrCreateObjectClassDelegateForNSClass(cls);
        if (!classDelegate) {
            return classDelegate.moveError();
        }

        if (classDelegate.value()->getValueMarshaller() == nullptr) {
            Valdi::SimpleExceptionTracker exceptionTracker;
            auto schemaAndKey = _schemaRegistry->getSchemaAndKeyForIdentifier(classDelegate.value()->getIdentifier());
            SC_ASSERT(schemaAndKey.has_value());
            auto valueMarshallerAndSchema = _valueMarshallerRegistry.getValueMarshaller(schemaAndKey.value().schemaKey, schemaAndKey.value().schema, exceptionTracker);
            if (!exceptionTracker) {
                return exceptionTracker.extractError();
            }

            classDelegate.value()->setValueMarshaller(valueMarshallerAndSchema.valueMarshaller.get());
            classDelegate.value()->setSchema(valueMarshallerAndSchema.schema.getClassRef().get());
        }

        return classDelegate;
    }

};

ValueSchemaRegistryListenerImpl::ValueSchemaRegistryListenerImpl(ObjCMarshallableObjectRegistry &registry): _registry(registry) {}

Valdi::Result<Valdi::Void> ValueSchemaRegistryListenerImpl::resolveSchemaIdentifierForSchemaKey(Valdi::ValueSchemaRegistry& registry,
                                                                                                                                  const Valdi::ValueSchemaRegistryKey& registryKey) {
    if (!registryKey.getSchemaKey().isTypeReference()) {
        return Valdi::Error("Not a type reference");
    }

    auto typeReference = registryKey.getSchemaKey().getTypeReference();
    if (!typeReference.isNamed()) {
        return Valdi::Error("Not a named type reference");
    }

    NSString *className = ValdiIOS::NSStringFromString(typeReference.getName());
    Class cls = NSClassFromString(className);

    if (cls && [cls respondsToSelector:@selector(valdiMarshallableObjectDescriptor)]) {
        auto result = _registry.registerClass(cls, [cls valdiMarshallableObjectDescriptor]);
        if (!result) {
            return result.moveError();
        }
        return Valdi::Void();
    }

    className = [className stringByAppendingString:VALDI_ENUM_CLASS_SUFFIX];
    cls = NSClassFromString(className);

    if (cls) {
        id<SCValdiEnum> valdiEnum = [cls new];
        auto result = _registry.registerEnum(valdiEnum);
        if (!result) {
            return result.moveError();
        }
        return Valdi::Void();
    }

    return Valdi::Error("Cannot resolve Objective-C class");
}

};

@implementation SCValdiMarshallableObjectRegistry {
    Valdi::Ref<ValdiIOS::ObjCMarshallableObjectRegistry> _registry;
}

- (instancetype)init
{
    self = [super init];

    if (self) {
        _registry = Valdi::makeShared<ValdiIOS::ObjCMarshallableObjectRegistry>(self);

        auto lock =_registry->lock();

        _registry->registerBlockHelper("o:v", SCValdiFunctionInvokeO_v, SCValdiBlockCreateO_v);
        _registry->registerBlockHelper("o:v", SCValdiFunctionInvokeO_v, SCValdiBlockCreateO_v);
        _registry->registerBlockHelper("oo:v", SCValdiFunctionInvokeOO_v, SCValdiBlockCreateOO_v);
        _registry->registerBlockHelper("ooo:v", SCValdiFunctionInvokeOOO_v, SCValdiBlockCreateOOO_v);
        _registry->registerBlockHelper("oooo:v", SCValdiFunctionInvokeOOOO_v, SCValdiBlockCreateOOOO_v);
        _registry->registerBlockHelper("ooooo:v", SCValdiFunctionInvokeOOOOO_v, SCValdiBlockCreateOOOOO_v);
        _registry->registerBlockHelper("oooooo:v", SCValdiFunctionInvokeOOOOOO_v, SCValdiBlockCreateOOOOOO_v);

        _registry->registerBlockHelper("o:o", SCValdiFunctionInvokeO_o, SCValdiBlockCreateO_o);
        _registry->registerBlockHelper("oo:o", SCValdiFunctionInvokeOO_o, SCValdiBlockCreateOO_o);
        _registry->registerBlockHelper("ooo:o", SCValdiFunctionInvokeOOO_o, SCValdiBlockCreateOOO_o);
        _registry->registerBlockHelper("oooo:o", SCValdiFunctionInvokeOOOO_o, SCValdiBlockCreateOOOO_o);

        _registry->registerBlockHelper("o:d", SCValdiFunctionInvokeO_d, SCValdiBlockCreateO_d);
        _registry->registerBlockHelper("oo:d", SCValdiFunctionInvokeOO_d, SCValdiBlockCreateOO_d);
        _registry->registerBlockHelper("ooo:d", SCValdiFunctionInvokeOOO_d, SCValdiBlockCreateOOO_d);

        _registry->registerBlockHelper("o:b", SCValdiFunctionInvokeO_b, SCValdiBlockCreateO_b);
        _registry->registerBlockHelper("oo:b", SCValdiFunctionInvokeOO_b, SCValdiBlockCreateOO_b);
        _registry->registerBlockHelper("ooo:b", SCValdiFunctionInvokeOOO_b, SCValdiBlockCreateOOO_b);

        // Register the default untyped classes and protocols
        [self registerUntypedClass:[NSObject class]];
        [self registerUntypedClass:[SCNValdiCoreAsset class]];
    }

    return self;
}

- (void)registerClass:(Class)objectClass objectDescriptor:(SCValdiMarshallableObjectDescriptor)objectDescriptor
{
    auto result = _registry->registerClass(objectClass, objectDescriptor);
    ValdiIOS::checkResult(result);
}

- (void)registerEnum:(id<SCValdiEnum>)valdiEnum
{
    auto result = _registry->registerEnum(valdiEnum);
    ValdiIOS::checkResult(result);
}

- (void)registerUntypedClass:(Class)objectClass
{
    _registry->registerUntyped(NSStringFromClass(objectClass));
}

- (void)registerUntypedProtocol:(Protocol *)protocol
{
    _registry->registerUntyped(NSStringFromProtocol(protocol));
}

- (void)forceLoadClass:(Class)objectClass
{
    auto result = _registry->forceLoadObjectClass(objectClass);
    ValdiIOS::checkResult(result);
}

- (NSInteger)marshallObject:(id)object
                    ofClass:(Class)objectClass
               toMarshaller:(SCValdiMarshallerRef)marshaller
{
    auto result = _registry->marshallObjectOfClass(objectClass, object, *SCValdiMarshallerUnwrap(marshaller));
    ValdiIOS::checkResult(result);
    return result.value();
}

- (NSInteger)marshallObject:(id)object toMarshaller:(SCValdiMarshallerRef)marshaller
{
    return [self marshallObject:object ofClass:[object class] toMarshaller:marshaller];
}

- (id)unmarshallObjectOfClass:(Class)objectClass
               fromMarshaller:(SCValdiMarshallerRef)marshaller
                      atIndex:(NSInteger)index
{
    auto result = _registry->unmarshallObjectOfClass(objectClass, *SCValdiMarshallerUnwrap(marshaller), static_cast<int>(index));
    ValdiIOS::checkResult(result);
    return result.value().getObject();
}

- (void *)allocateStorageForClass:(Class)cls
{
    auto result = _registry->getOrCreateObjectClassDelegateForNSClass(cls);
    ValdiIOS::checkResult(result);
    return result.value()->allocateFieldsStorage();
}

- (void *)allocateStorageForClass:(Class)cls fieldValues:(va_list)fieldValues
{
    auto result = _registry->getOrCreateObjectClassDelegateForNSClass(cls);
    ValdiIOS::checkResult(result);
    return result.value()->allocateFieldsStorageWithValues(fieldValues);
}

- (void)deallocateStorage:(void *)storage forClass:(Class)cls
{
    auto result = _registry->getOrCreateObjectClassDelegateForNSClass(cls);
    ValdiIOS::checkResult(result);
    result.value()->deallocateFieldsStorage((SCValdiFieldValue *)storage);
}

- (void)setSchemaOfClass:(Class)objectClass inMarshaller:(SCValdiMarshallerRef)marshaller
{
    auto result = _registry->setActiveSchemaOfClassInMarshaller(objectClass, *SCValdiMarshallerUnwrap(marshaller));
    ValdiIOS::checkResult(result);
}

- (id)makeObjectOfClass:(Class)objectClass
{
    auto result = _registry->getOrCreateObjectClassDelegateForNSClass(objectClass);
    ValdiIOS::checkResult(result);

    return [[objectClass alloc] initWithObjectRegistry:self storage:result.value()->allocateFieldsStorage()];
}

- (id)makeObjectWithFieldValuesOfClass:(Class)objectClass, ...
{
    va_list v;
    va_start(v, objectClass);
    void *storage = [self allocateStorageForClass:objectClass fieldValues:v];
    va_end(v);
    return [[objectClass alloc] initWithObjectRegistry:self storage:storage];
}

- (BOOL)object:(id)object equalsToObject:(id)otherObject forClass:(Class)cls
{
    auto result = _registry->getOrCreateObjectClassDelegateForNSClass(cls);
    ValdiIOS::checkResult(result);

    return SCValdiMarshallableObjectsEqual(object, otherObject, (NSUInteger)result.value()->getFieldsCount(), result.value()->getFieldDescriptors());
}

- (void *)getValueSchemaRegistryPtr
{
    return _registry->getValueSchemaRegistryPtr();
}

@end

SCValdiMarshallableObjectRegistry *SCValdiMarshallableObjectRegistryGetSharedInstance()
{
    static dispatch_once_t onceToken;
    static SCValdiMarshallableObjectRegistry *registry;
    dispatch_once(&onceToken, ^{
        registry = [SCValdiMarshallableObjectRegistry new];
    });
    return registry;
}

void SCValdiMarshallerObjectRegistryRegisterProxyIfNeeded(id object, Protocol *proxyProtocol)
{
    if ([object respondsToSelector:@selector(pushToValdiMarshaller:)]) {
        return;
    }

    NSString *protocolName = NSStringFromProtocol(proxyProtocol);
    Class proxyClass = NSClassFromString(protocolName);
    if (!proxyClass) {
        [NSException raise:NSInternalInconsistencyException format:@"Could not resolve proxy class for protocol %@", protocolName];
        return;
    }

    Method method = class_getInstanceMethod(proxyClass, @selector(pushToValdiMarshaller:));
    IMP block = imp_implementationWithBlock(^NSInteger(__unsafe_unretained id receiver, SCValdiMarshallerRef marshaller) {
        return [SCValdiMarshallableObjectRegistryGetSharedInstance() marshallObject:receiver ofClass:proxyClass toMarshaller:marshaller];
    });

    class_addMethod([object class], @selector(pushToValdiMarshaller:), block, method_getTypeEncoding(method));
}
