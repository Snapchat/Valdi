//
//  ValueSchema.hpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 1/17/23.
//

#pragma once

#include "valdi_core/cpp/Schema/ValueSchemaTypeReference.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include <optional>
#include <string>

namespace Valdi {

template<typename T, typename ValueType>
struct InlineContainerAllocator;

class ValueFunctionSchema;
class ClassSchema;
struct ClassPropertySchema;
class ValueSchema;
class GenericValueSchemaTypeReference;
class ValueSchemaReference;
class EnumSchema;
class ArraySchema;
class MapSchema;
class PromiseSchema;
class ES6MapSchema;
class ES6SetSchema;
class OutcomeSchema;
class ProtoSchema;
class ValueFunctionSchemaAttributes;
struct EnumCaseSchema;

enum class ValueSchemaPrintFormat {
    HumanReadable,
    Short,
};

class ValueFunctionSchemaAttributes {
public:
    ValueFunctionSchemaAttributes();
    ValueFunctionSchemaAttributes(bool isMethod, bool isSingleCall, bool shouldDispatchToWorkerThread);
    ValueFunctionSchemaAttributes(bool isMethod,
                                  bool isSingleCall,
                                  bool shouldDispatchToWorkerThread,
                                  bool allowSyncCall);

    bool empty() const;

    bool isMethod() const;
    bool isSingleCall() const;
    bool shouldDispatchToWorkerThread() const;
    /** True if sync call is allowed (either module is not async-strict, or function has @AllowSyncCall). */
    bool allowSyncCall() const;

    bool operator==(const ValueFunctionSchemaAttributes& other) const;
    bool operator!=(const ValueFunctionSchemaAttributes& other) const;

private:
    bool _isMethod = false;
    bool _isSingleCall = false;
    bool _shouldDispatchToWorkerThread = false;
    /** Default true to limit impact on existing code; set false only when async_strict_mode and no @AllowSyncCall. */
    bool _allowSyncCall = true;
};

/**
 ValueSchema is generic type container that handles types that the Value and ValueFunction API supports.
 It can represent the definition of a model, a function or other types. ValueSchema only deals with
 types, not with values.

 Each supported type has a string representation which can be written as human readable or shortened.
 The following list shows all the supported types, with their human readable and shortened string
 representation, and an explanation of what each type represents:

   Untyped:
     'untyped', 'u'
     Represents a lack of typing

   String:
     'string', 's'
     A string type

   Integer:
     'int', 'i'
     A 32 bits integer type

   LongInteger:
     'long', 'l'
     A 64 bits integer type

   Double:
     'double', 'd'
     A 64 bits double precision floating type

   Boolean:
     'boolean, 'b'
     A boolean type

   ValueTypedArray:
     'typedarray', 't'
     Represents a ValueTypedArray type, which is a bytes container, like Uint8Array or ArrayBuffer in JavaScript.

   Array:
     'array', 'a'
     An array of another type. The element type is written with surrounding <> like `array<string>`

   Map:
     'map', 'm'
     A typed map. The map key and value type is written with surrounding <> like `map<string, int>`

   Function:
     'func', 'f'
     A function type. Parameter types are written with surrounding (), followed by the return type
     with a ':' as separator. For example: `func(string): boolean`, which represents a function taking
     one string argument type and returning a boolean type. A function type can have a modifier character
     that can be put between 'func' / 'f' and the parameter types within (). The modifier '*' tells that
     the function represents a method of an object, whereas the modifier '!' tells that the function is
     "single call", meaning it can only be called once.

   Promise:
     'promise', 'p'
     A promise type over a value. The value type of the promise is writtein with surrounding <> like `promise<string>`

   TypeReference:
     'ref', 'r'
     A reference to another type. A type reference can be positional or named. Positional type references
     refer to generic arguments, for example in `Class<T, F>`, T will be a positional type reference at index 0,
     whereas F will be a positional type reference at index 1. Named type references refer to another type that
     should be resolved by the ValueSchemaRegistry. For example in a property like 'myProperty: User', 'User'
     will be a named type 'User'. Type references values are written after a ':' delimiter. Named type references
     are written with surrounding quotes. For example: `ref:1` is a positional type reference at index 1,
     `ref:'User'` is a named type reference for a type named 'User'.

   GenericTypeReference:
     'genref', 'g'
     A generic reference to another type. It is a specialized form of TypeReference that has type arguments.
     Type arguments are written with surrounding <> after the target type reference was written.
     For example, `genref:'Observable'<string>` represents a generic type reference on a type named 'Observable',
     which has one generic type argument of type 'string'.

   SchemaReference:
     'link', 'l'
     A resolved reference to another schema. The ValueSchemaRegistry will resolve TypeReference's and
 GenericTypeReference's inside ValueSchema objects in order to replace them with SchemaReference's. Once all the type
 references have been replaced by schema references, the ValueSchema object itself contains the whole API. Schema
 references cannot be parsed, as they rely on the registry injecting a live reference to the schema. For example,
 `link:ref:'User'` represents a schema reference to the User schema. When "recurseInSchemaReferences" is passed as true
 to the toString() function, the referenced schema itself will be printed instead of the schema reference.

   Class:
     'class', 'c'
     A class type that is named and contains properties. The name of the class is added after a space separator,
 followed by the properties, written in enclosing brackets {}. Each property has a name with a type. For example, `class
 'User'{'username': string}` represents a class named User which has a property named 'username' of type string.

   Enum:
     'enum', 'e'
    An enum type that is named and contains enum values. The type of the enum is added inside brackets right after
    the enum declaration. The name of the enum is then added, followed by the allowed values inside braces.
    For example, `enum<int> 'StatusCode' {"ok": 200, "not_found": 404, "internal_error": 500}` represents an
    integer enum named `StatusCode` with the allowed values 200, 404, 500.

    Optional:
     '?'
     Modifier added optionally after a type, which tells that the value is allowed to be null or undefined.
     For example, `string?` represents an optional string, whereas `array?<bool?>` represents an optional
    array where each item is an optional boolean.

    Boxed:
     '@'
     Modifier added optionally after a type, which tells that the value is boxed in a heap allocated container.
     For example, `b@` represents a boxoed boolean, which will have a different runtime representation than a non
     boxed `b` in some languages like Java and Objective-C
 */
class ValueSchema {
public:
    ValueSchema();
    ValueSchema(const ValueSchema& other);
    ValueSchema(ValueSchema&& other) noexcept;
    ~ValueSchema();

    std::string toString() const;
    std::string toString(ValueSchemaPrintFormat format, bool recurseInSchemaReferences) const;

    ValueSchema asOptional() const;
    ValueSchema asNonOptional() const;
    ValueSchema asArray() const;
    ValueSchema asBoxed() const;

    bool isUntyped() const;
    bool isVoid() const;
    bool isOptional() const;
    bool isString() const;
    bool isInteger() const;
    bool isLongInteger() const;
    bool isDouble() const;
    bool isBoolean() const;
    bool isFunction() const;
    bool isClass() const;
    bool isEnum() const;
    bool isValueTypedArray() const;
    bool isArray() const;
    bool isMap() const;
    bool isES6Map() const;
    bool isES6Set() const;
    bool isOutcome() const;
    bool isDate() const;
    bool isProto() const;
    bool isPromise() const;
    bool isTypeReference() const;
    bool isGenericTypeReference() const;
    bool isSchemaReference() const;
    /**
     Whether this ValueSchema should always be in a boxed type.
     In Objective-C a boxed type for a numbeer type would be NSNumber for instance,
     in Java a number type would be Integer or Double etc...
     */
    bool isBoxed() const;

    const ValueFunctionSchema* getFunction() const;
    Ref<ValueFunctionSchema> getFunctionRef() const;
    const ClassSchema* getClass() const;
    Ref<ClassSchema> getClassRef() const;
    const EnumSchema* getEnum() const;
    Ref<EnumSchema> getEnumRef() const;
    const ArraySchema* getArray() const;
    const PromiseSchema* getPromise() const;
    const MapSchema* getMap() const;
    const ES6MapSchema* getES6Map() const;
    const ES6SetSchema* getES6Set() const;
    const OutcomeSchema* getOutcome() const;
    const ProtoSchema* getProto() const;

    ValueSchemaTypeReference getTypeReference() const;
    const ValueSchemaReference* getSchemaReference() const;
    const GenericValueSchemaTypeReference* getGenericTypeReference() const;

    ValueSchema getReferencedSchema() const;

    ValueSchema getArrayItemSchema() const;

    static ValueSchema untyped();
    static ValueSchema voidType();
    static ValueSchema integer();
    static ValueSchema longInteger();
    static ValueSchema doublePrecision();
    static ValueSchema boolean();
    static ValueSchema string();
    static ValueSchema valueTypedArray();

    template<typename T>
    static ValueSchema primitiveType();
    template<>
    ValueSchema primitiveType<bool>() {
        return boolean();
    }
    template<>
    ValueSchema primitiveType<int32_t>() {
        return integer();
    }
    template<>
    ValueSchema primitiveType<int64_t>() {
        return longInteger();
    }
    template<>
    ValueSchema primitiveType<double>() {
        return doublePrecision();
    }
    template<>
    ValueSchema primitiveType<std::string>() {
        return string();
    }

    static ValueSchema typeReference(const ValueSchemaTypeReference& reference);

    static ValueSchema genericTypeReference(const ValueSchemaTypeReference& type,
                                            const ValueSchema* typeArguments,
                                            size_t typeArgumentsSize);
    static ValueSchema genericTypeReference(const ValueSchemaTypeReference& type,
                                            std::initializer_list<ValueSchema> typeArguments) {
        return genericTypeReference(type, typeArguments.begin(), typeArguments.size());
    }

    static ValueSchema cls(const StringBox& className,
                           bool isInterface,
                           const ClassPropertySchema* properties,
                           size_t propertiesSize);

    static ValueSchema cls(const StringBox& className,
                           bool isInterface,
                           std::initializer_list<ClassPropertySchema> properties) {
        return cls(className, isInterface, properties.begin(), properties.size());
    }

    static ValueSchema cls(const Ref<ClassSchema>& classSchema);

    static ValueSchema enumeration(const StringBox& name,
                                   const ValueSchema& caseSchema,
                                   const EnumCaseSchema* cases,
                                   size_t casesSize);

    static ValueSchema enumeration(const StringBox& name,
                                   const ValueSchema& caseSchema,
                                   std::initializer_list<EnumCaseSchema> cases) {
        return enumeration(name, caseSchema, cases.begin(), cases.size());
    }

    static ValueSchema function(const ValueFunctionSchemaAttributes& attributes,
                                const ValueSchema& returnValue,
                                const ValueSchema* parameters,
                                size_t parametersSize);

    static ValueSchema function(const ValueFunctionSchemaAttributes& attributes,
                                const ValueSchema& returnValue,
                                std::initializer_list<ValueSchema> parameters) {
        return function(attributes, returnValue, parameters.begin(), parameters.size());
    }

    static ValueSchema function(const ValueSchema& returnValue, std::initializer_list<ValueSchema> parameters) {
        return function(ValueFunctionSchemaAttributes(), returnValue, parameters.begin(), parameters.size());
    }

    static ValueSchema array(const ValueSchema& itemSchema);
    static ValueSchema array(ValueSchema&& itemSchema);
    static ValueSchema map(const ValueSchema& keySchema, const ValueSchema& valueSchema);
    static ValueSchema map(ValueSchema&& keySchema, ValueSchema&& valueSchema);
    static ValueSchema promise(const ValueSchema& valueSchema);
    static ValueSchema promise(ValueSchema&& valueSchema);
    static ValueSchema optional(const ValueSchema& itemSchema);
    static ValueSchema optional(ValueSchema&& itemSchema);
    static ValueSchema boxed(const ValueSchema& itemSchema);
    static ValueSchema boxed(ValueSchema&& itemSchema);
    static ValueSchema nonOptional(const ValueSchema& itemSchema);
    static ValueSchema schemaReference(const Ref<ValueSchemaReference>& schemaReference);
    static ValueSchema es6map(const ValueSchema& keySchema, const ValueSchema& valueSchema);
    static ValueSchema es6set(const ValueSchema& keySchema);
    static ValueSchema outcome(const ValueSchema& valueSchema, const ValueSchema& errorSchema);
    static ValueSchema date();
    static ValueSchema proto(const std::string_view* classNamePartsBegin, const std::string_view* classNamePartsEnd);

    static Result<ValueSchema> parse(std::string_view str);

    ValueSchema& operator=(const ValueSchema& other);
    ValueSchema& operator=(ValueSchema&& other) noexcept;

    bool operator==(const ValueSchema& other) const;
    bool operator!=(const ValueSchema& other) const;

    size_t hash() const;

    std::ostream& outputToStream(std::ostream& os,
                                 FlatSet<const ValueSchemaReference*>& seenSchemaReferences,
                                 ValueSchemaPrintFormat format,
                                 bool recurseInSchemaReferences) const;

private:
    enum ValueSchemaInnerType : uint8_t {
        ValueSchemaInnerTypeUntyped = 0,
        ValueSchemaInnerTypeVoid,
        ValueSchemaInnerTypeInt,
        ValueSchemaInnerTypeLong,
        ValueSchemaInnerTypeDouble,
        ValueSchemaInnerTypeBoolean,
        ValueSchemaInnerTypeString,
        ValueSchemaInnerTypeValueTypedArray,
        ValueSchemaInnerTypeNamedTypeReference,
        ValueSchemaInnerTypePositionalTypeReference,
        ValueSchemaInnerTypeGenericTypeReference,
        ValueSchemaInnerTypeClass,
        ValueSchemaInnerTypeEnum,
        ValueSchemaInnerTypeFunction,
        ValueSchemaInnerTypeArray,
        ValueSchemaInnerTypeMap,
        ValueSchemaInnerTypePromise,
        ValueSchemaInnerTypeSchemaReference,
        ValueSchemaInnerTypeES6Map,
        ValueSchemaInnerTypeES6Set,
        ValueSchemaInnerTypeOutcome,
        ValueSchemaInnerTypeDate,
        ValueSchemaInnerTypeProto,
    };

    ValueSchema(ValueSchemaInnerType type, uint8_t flags, uint8_t typeReferenceFlags, Ref<RefCountable> ptr);

    ValueSchemaInnerType _type = ValueSchemaInnerTypeUntyped;
    uint8_t _flags = 0;
    uint8_t _typeReferenceFlags = 0;
    Ref<RefCountable> _ptr;

    void sanityCheck() const;
};

std::ostream& operator<<(std::ostream& os, const ValueSchema& value);

/**
 Represent a reference to another schema.
 */
class ValueSchemaReference : public SimpleRefCountable {
public:
    virtual ValueSchema getKey() const = 0;
    virtual ValueSchema getSchema() const = 0;
};

class ArraySchema : public SimpleRefCountable {
public:
    explicit ArraySchema(ValueSchema&& elementSchema);
    explicit ArraySchema(const ValueSchema& elementSchema);
    ~ArraySchema() override;

    const ValueSchema& getElementSchema() const;

private:
    ValueSchema _elementSchema;
};

class MapSchema : public SimpleRefCountable {
public:
    MapSchema(ValueSchema&& key, ValueSchema&& value);
    MapSchema(const ValueSchema& key, const ValueSchema& value);
    ~MapSchema() override;

    const ValueSchema& getKey() const;
    const ValueSchema& getValue() const;

private:
    ValueSchema _key;
    ValueSchema _value;
};

class ES6MapSchema : public SimpleRefCountable {
public:
    ES6MapSchema(ValueSchema&& key, ValueSchema&& value);
    ES6MapSchema(const ValueSchema& key, const ValueSchema& value);
    ~ES6MapSchema() override;

    const ValueSchema& getKey() const;
    const ValueSchema& getValue() const;

private:
    ValueSchema _key;
    ValueSchema _value;
};

class ES6SetSchema : public SimpleRefCountable {
public:
    explicit ES6SetSchema(ValueSchema&& key);
    explicit ES6SetSchema(const ValueSchema& key);
    ~ES6SetSchema() override;

    const ValueSchema& getKey() const;

private:
    ValueSchema _key;
};

class OutcomeSchema : public SimpleRefCountable {
public:
    OutcomeSchema(ValueSchema&& valueSchema, ValueSchema&& errorSchema);
    OutcomeSchema(const ValueSchema& valueSchema, const ValueSchema& errorSchema);
    ~OutcomeSchema() override;

    const ValueSchema& getValue() const;
    const ValueSchema& getError() const;

private:
    ValueSchema _value;
    ValueSchema _error;
};

class ProtoSchema : public SimpleRefCountable {
public:
    ProtoSchema(const std::string_view* classNamePartsBegin, const std::string_view* classNamePartsEnd);
    ~ProtoSchema() override;

    size_t hash() const;

    bool operator==(const ProtoSchema& other) const;
    bool operator!=(const ProtoSchema& other) const;

    const std::vector<std::string_view>& classNameParts() const {
        return _classNameParts;
    }

private:
    const std::vector<std::string_view> _classNameParts;
};

class PromiseSchema : public SimpleRefCountable {
public:
    explicit PromiseSchema(ValueSchema&& valueSchema);
    explicit PromiseSchema(const ValueSchema& valueSchema);
    ~PromiseSchema() override;

    const ValueSchema& getValueSchema() const;

private:
    ValueSchema _valueSchema;
};

struct ClassPropertySchema {
    StringBox name;
    ValueSchema schema;

    inline ClassPropertySchema() = default;
    inline ClassPropertySchema(const StringBox& name, const ValueSchema& schema) : name(name), schema(schema) {}
    inline ClassPropertySchema(StringBox&& name, ValueSchema&& schema)
        : name(std::move(name)), schema(std::move(schema)) {}
};

class ClassSchema : public SimpleRefCountable {
public:
    ~ClassSchema() override;

    const StringBox& getClassName() const;
    bool isInterface() const;

    const ClassPropertySchema* getProperties() const;
    size_t getPropertiesSize() const;
    const ClassPropertySchema& getProperty(size_t index) const;
    ClassPropertySchema& getProperty(size_t index);

    std::optional<size_t> getPropertyIndexForName(const StringBox& propertyName) const;

    size_t hash() const;

    const ClassPropertySchema* begin() const;
    const ClassPropertySchema* end() const;

    bool operator==(const ClassSchema& other) const;
    bool operator!=(const ClassSchema& other) const;

    static Ref<ClassSchema> make(const StringBox& className, bool isInterface, size_t propertiesSize);

private:
    StringBox _className;
    bool _isInterface;
    size_t _propertiesSize;

    friend InlineContainerAllocator<ClassSchema, ClassPropertySchema>;

    ClassSchema(const StringBox& className, bool isInterface, size_t propertiesSize);
};

struct EnumCaseSchema {
    StringBox name;
    Value value;

    inline EnumCaseSchema() = default;
    inline EnumCaseSchema(const StringBox& name, const Value& value) : name(name), value(value) {}
};

class EnumSchema : public SimpleRefCountable {
public:
    ~EnumSchema() override;

    const StringBox& getName() const;
    const ValueSchema& getCaseSchema() const;

    size_t getCasesSize() const;
    const EnumCaseSchema& getCase(size_t index) const;
    EnumCaseSchema& getCase(size_t index);

    const EnumCaseSchema* begin() const;
    const EnumCaseSchema* end() const;

    bool operator==(const EnumSchema& other) const;
    bool operator!=(const EnumSchema& other) const;

    static Ref<EnumSchema> make(const StringBox& name, const ValueSchema& caseSchema, size_t casesSize);

private:
    StringBox _name;
    ValueSchema _caseSchema;
    size_t _casesSize;

    friend InlineContainerAllocator<EnumSchema, EnumCaseSchema>;

    EnumSchema(const StringBox& name, const ValueSchema& caseSchema, size_t casesSize);
};

class ValueFunctionSchema : public SimpleRefCountable {
public:
    ~ValueFunctionSchema() override;

    ValueSchema& getReturnValue();
    const ValueSchema& getReturnValue() const;

    size_t getParametersSize() const;
    const ValueSchema& getParameter(size_t index) const;
    ValueSchema& getParameter(size_t index);
    const ValueSchema* getParameters() const;

    const ValueFunctionSchemaAttributes& getAttributes() const;

    bool operator==(const ValueFunctionSchema& other) const;
    bool operator!=(const ValueFunctionSchema& other) const;

    static Ref<ValueFunctionSchema> make(const ValueFunctionSchemaAttributes& attributes,
                                         const ValueSchema& returnValue,
                                         size_t parametersSize);

private:
    ValueFunctionSchemaAttributes _attributes;
    ValueSchema _returnValue;
    size_t _parametersSize;

    friend InlineContainerAllocator<ValueFunctionSchema, ValueSchema>;

    ValueFunctionSchema(const ValueFunctionSchemaAttributes& attributes,
                        const ValueSchema& returnValue,
                        size_t parametersSize);
};

class GenericValueSchemaTypeReference : public SimpleRefCountable {
public:
    ~GenericValueSchemaTypeReference() override;

    const ValueSchemaTypeReference& getType() const;

    size_t getTypeArgumentsSize() const;
    const ValueSchema* getTypeArguments() const;
    const ValueSchema& getTypeArgument(size_t index) const;
    ValueSchema& getTypeArgument(size_t index);

    bool operator==(const GenericValueSchemaTypeReference& other) const;
    bool operator!=(const GenericValueSchemaTypeReference& other) const;

    static Ref<GenericValueSchemaTypeReference> make(const ValueSchemaTypeReference& type, size_t typeArgumentsSize);

private:
    ValueSchemaTypeReference _type;
    size_t _typeArgumentsSize;

    friend InlineContainerAllocator<GenericValueSchemaTypeReference, ValueSchema>;

    GenericValueSchemaTypeReference(const ValueSchemaTypeReference& type, size_t typeArgumentsSize);
};

} // namespace Valdi
