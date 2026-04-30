//
//  ValueSchema.cpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 1/17/23.
//

#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaParser.hpp"
#include "valdi_core/cpp/Utils/InlineContainerAllocator.hpp"
#include <boost/functional/hash.hpp>
#include <cstdlib>
#include <sstream>

namespace Valdi {

constexpr uint8_t kOptionalFlagBit = 1 << 0;
constexpr uint8_t kBoxedFlagBit = 1 << 1;

static_assert(sizeof(ValueSchema) <= 16, "ValueSchema should be at most 16 bytes");

ValueSchema::ValueSchema() = default;

ValueSchema::ValueSchema(ValueSchemaInnerType type, uint8_t flags, uint8_t typeReferenceFlags, Ref<RefCountable> ptr)
    : _type(type), _flags(flags), _typeReferenceFlags(typeReferenceFlags), _ptr(std::move(ptr)) {
    sanityCheck();
}

ValueSchema::ValueSchema(const ValueSchema& other)
    : _type(other._type), _flags(other._flags), _typeReferenceFlags(other._typeReferenceFlags), _ptr(other._ptr) {
    sanityCheck();
}

ValueSchema::ValueSchema(ValueSchema&& other) noexcept
    : _type(other._type),
      _flags(other._flags),
      _typeReferenceFlags(other._typeReferenceFlags),
      _ptr(std::move(other._ptr)) {
    other._type = ValueSchemaInnerTypeUntyped;
    other._flags = 0;
    other._typeReferenceFlags = 0;

    sanityCheck();
}

ValueSchema::~ValueSchema() = default;

std::string ValueSchema::toString() const {
    return toString(ValueSchemaPrintFormat::HumanReadable, false);
}

std::string ValueSchema::toString(ValueSchemaPrintFormat format, bool recurseInSchemaReferences) const {
    std::ostringstream ss;
    FlatSet<const ValueSchemaReference*> seenSchemaReferences;
    this->outputToStream(ss, seenSchemaReferences, format, recurseInSchemaReferences);

    return ss.str();
}

static void printType(std::ostream& os, ValueSchemaPrintFormat format, std::string_view type, uint8_t flags) {
    if (format == ValueSchemaPrintFormat::HumanReadable) {
        os << type;
    } else {
        os << type[0];
    }
    if ((flags & kBoxedFlagBit) != 0) {
        os << "@";
    }
    if ((flags & kOptionalFlagBit) != 0) {
        os << "?";
    }
}

template<typename F>
static void printRangeWithDelimiters(
    std::ostream& os, size_t length, std::string_view delimiterStart, std::string_view delimiterEnd, F&& handler) {
    os << delimiterStart;
    for (size_t i = 0; i < length; i++) {
        if (i > 0) {
            os << ", ";
        }
        handler(i);
    }

    os << delimiterEnd;
}

template<typename TypeRefF,
         typename GenericRefF,
         typename ClassF,
         typename EnumF,
         typename FunctionF,
         typename ArrayF,
         typename MapF,
         typename ES6MapF,
         typename ES6SetF,
         typename OutcomeF,
         typename ProtoF,
         typename PromiseF,
         typename SchemaRefF>
inline static void visitNestedType(const ValueSchema& schema,
                                   TypeRefF&& typeRefF,
                                   GenericRefF&& genericRefF,
                                   ClassF&& classF,
                                   EnumF&& enumF,
                                   FunctionF&& functionF,
                                   ArrayF&& arrayF,
                                   MapF&& mapF,
                                   ES6MapF&& es6mapF,
                                   ES6SetF&& es6setF,
                                   OutcomeF&& outcomeF,
                                   ProtoF&& protoF,
                                   PromiseF&& promiseF,
                                   SchemaRefF&& schemaRefF) {
    if (schema.isTypeReference()) {
        typeRefF(schema.getTypeReference());
    } else if (schema.isGenericTypeReference()) {
        genericRefF(*schema.getGenericTypeReference());
    } else if (schema.isClass()) {
        classF(*schema.getClass());
    } else if (schema.isEnum()) {
        enumF(*schema.getEnum());
    } else if (schema.isFunction()) {
        functionF(*schema.getFunction());
    } else if (schema.isArray()) {
        arrayF(*schema.getArray());
    } else if (schema.isMap()) {
        mapF(*schema.getMap());
    } else if (schema.isES6Map()) {
        es6mapF(*schema.getES6Map());
    } else if (schema.isES6Set()) {
        es6setF(*schema.getES6Set());
    } else if (schema.isOutcome()) {
        outcomeF(*schema.getOutcome());
    } else if (schema.isProto()) {
        protoF(*schema.getProto());
    } else if (schema.isPromise()) {
        promiseF(*schema.getPromise());
    } else if (schema.isSchemaReference()) {
        schemaRefF(*schema.getSchemaReference());
    }
}

static void printTypeReference(std::ostream& os,
                               const ValueSchemaTypeReference& typeReference,
                               ValueSchemaPrintFormat format) {
    if (!typeReference.isPositional()) {
        switch (typeReference.getTypeHint()) {
            case ValueSchemaTypeReferenceTypeHintUnknown:
                break;
            case ValueSchemaTypeReferenceTypeHintObject:
                os << "<";
                printType(os, format, kValueSchemaTypeReferenceTypeHintObjectStr, 0);
                os << ">";
                break;
            case ValueSchemaTypeReferenceTypeHintEnum:
                os << "<";
                printType(os, format, kValueSchemaTypeReferenceTypeHintEnumStr, 0);
                os << ">";
                break;
            case ValueSchemaTypeReferenceTypeHintConverted:
                os << "<";
                printType(os, format, kValueSchemaTypeReferenceTypeHintConvertedStr, 0);
                os << ">";
                break;
        }
    }

    os << ":" << typeReference.toString();
}

static void printAttributeIfNeeded(std::ostream& os, bool value, std::string_view str, bool& didPrint) {
    if (!value) {
        return;
    }

    if (!didPrint) {
        didPrint = true;
    } else {
        os << ", ";
    }

    os << str;
}

std::ostream& ValueSchema::outputToStream(std::ostream& os,
                                          FlatSet<const ValueSchemaReference*>& seenSchemaReferences,
                                          ValueSchemaPrintFormat format,
                                          bool recurseInSchemaReferences) const {
    switch (_type) {
        case ValueSchemaInnerTypeUntyped:
            printType(os, format, kStringTypeUntyped, _flags);
            break;
        case ValueSchemaInnerTypeVoid:
            printType(os, format, kStringTypeVoid, _flags);
            break;
        case ValueSchemaInnerTypeInt:
            printType(os, format, kStringTypeInt, _flags);
            break;
        case ValueSchemaInnerTypeLong:
            printType(os, format, kStringTypeLong, _flags);
            break;
        case ValueSchemaInnerTypeDouble:
            printType(os, format, kStringTypeDouble, _flags);
            break;
        case ValueSchemaInnerTypeBoolean:
            printType(os, format, kStringTypeBool, _flags);
            break;
        case ValueSchemaInnerTypeString:
            printType(os, format, kStringTypeString, _flags);
            break;
        case ValueSchemaInnerTypeValueTypedArray:
            printType(os, format, kStringTypeValueTypedArray, _flags);
            break;
        case ValueSchemaInnerTypeNamedTypeReference:
        case ValueSchemaInnerTypePositionalTypeReference:
            printType(os, format, kStringTypeTypeReference, _flags);
            printTypeReference(os, getTypeReference(), format);
            break;
        case ValueSchemaInnerTypeGenericTypeReference: {
            printType(os, format, kStringTypeGenericTypeReference, _flags);
            const auto* genericTypeReference = getGenericTypeReference();
            printTypeReference(os, genericTypeReference->getType(), format);
            printRangeWithDelimiters(os, genericTypeReference->getTypeArgumentsSize(), "<", ">", [&](size_t index) {
                genericTypeReference->getTypeArgument(index).outputToStream(
                    os, seenSchemaReferences, format, recurseInSchemaReferences);
            });
        } break;
        case ValueSchemaInnerTypeClass: {
            const auto* cls = getClass();
            printType(os, format, kStringTypeClass, _flags);
            if (cls->isInterface()) {
                os << kStringTypeClassModifierInterface;
            }
            os << " '" << cls->getClassName().toStringView() << "'";
            printRangeWithDelimiters(os, cls->getPropertiesSize(), "{", "}", [&](size_t index) {
                const auto& property = cls->getProperty(index);
                os << "'";
                os << property.name.toStringView();
                os << "': ";
                property.schema.outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            });
        } break;
        case ValueSchemaInnerTypeEnum: {
            printType(os, format, kStringTypeEnum, _flags);
            const auto* enumeration = getEnum();
            os << "<";
            enumeration->getCaseSchema().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ">";
            os << " '" << enumeration->getName().toStringView() << "'";
            printRangeWithDelimiters(os, enumeration->getCasesSize(), "{", "}", [&](size_t index) {
                const auto& enumCase = enumeration->getCase(index);
                os << "'";
                os << enumCase.name.toStringView();
                os << "': ";
                if (enumCase.value.isString()) {
                    os << "'";
                    os << enumCase.value.toStringBox().toStringView();
                    os << "'";
                } else {
                    os << enumCase.value.toString();
                }
            });
        } break;
        case ValueSchemaInnerTypeFunction: {
            printType(os, format, kStringTypeFunction, _flags);
            const auto* function = getFunction();
            const auto& attributes = function->getAttributes();

            if (!attributes.empty()) {
                os << "|";
                bool hasPrev = false;
                printAttributeIfNeeded(os, attributes.isMethod(), kStringTypeFunctionAttributeMethod, hasPrev);
                printAttributeIfNeeded(os, attributes.isSingleCall(), kStringTypeFunctionAttributeSingleCall, hasPrev);
                printAttributeIfNeeded(
                    os, attributes.shouldDispatchToWorkerThread(), kStringTypeFunctionAttributeWorkerThread, hasPrev);
                printAttributeIfNeeded(os, !attributes.allowSyncCall(), kStringTypeFunctionAttributeBanSync, hasPrev);

                os << "|";
            }

            printRangeWithDelimiters(os, function->getParametersSize(), "(", ")", [&](size_t index) {
                function->getParameter(index).outputToStream(
                    os, seenSchemaReferences, format, recurseInSchemaReferences);
            });
            os << ": ";
            function->getReturnValue().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
        } break;
        case ValueSchemaInnerTypeArray:
            printType(os, format, kStringTypeArray, _flags);
            os << "<";
            getArrayItemSchema().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ">";
            break;
        case ValueSchemaInnerTypeMap: {
            printType(os, format, kStringTypeMap, _flags);
            const auto* map = getMap();
            os << "<";
            map->getKey().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ", ";
            map->getValue().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ">";
        } break;
        case ValueSchemaInnerTypeES6Map: {
            printType(os, format, kStringTypeES6Map, _flags);
            const auto* map = getES6Map();
            os << "<";
            map->getKey().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ", ";
            map->getValue().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ">";
        } break;
        case ValueSchemaInnerTypeES6Set: {
            printType(os, format, kStringTypeES6Set, _flags);
            const auto* map = getES6Set();
            os << "<";
            map->getKey().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ">";
        } break;
        case ValueSchemaInnerTypeOutcome: {
            printType(os, format, kStringTypeOutcome, _flags);
            const auto* outcome = getOutcome();
            os << "<";
            outcome->getValue().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ", ";
            outcome->getError().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ">";
        } break;
        case ValueSchemaInnerTypeDate:
            printType(os, format, kStringTypeDate, _flags);
            break;
        case ValueSchemaInnerTypeProto: {
            printType(os, format, kStringTypeProto, _flags);
            const auto* proto = getProto();
            const auto& parts = proto->classNameParts();
            printRangeWithDelimiters(os, parts.size(), "<", ">", [&](size_t i) { os << parts[i]; });
        } break;
        case ValueSchemaInnerTypePromise: {
            printType(os, format, kStringTypePromise, _flags);
            const auto* promise = getPromise();
            os << "<";
            promise->getValueSchema().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            os << ">";
        } break;
        case ValueSchemaInnerTypeSchemaReference: {
            printType(os, format, kStringTypeSchemaRef, _flags);
            os << ":";
            const auto* schemaReference = getSchemaReference();
            if (recurseInSchemaReferences) {
                if (seenSchemaReferences.find(schemaReference) == seenSchemaReferences.end()) {
                    seenSchemaReferences.insert(schemaReference);
                    schemaReference->getSchema().outputToStream(
                        os, seenSchemaReferences, format, recurseInSchemaReferences);
                } else {
                    schemaReference->getKey().outputToStream(
                        os, seenSchemaReferences, format, recurseInSchemaReferences);
                }
            } else {
                schemaReference->getKey().outputToStream(os, seenSchemaReferences, format, recurseInSchemaReferences);
            }
        } break;
    }

    return os;
}

Result<ValueSchema> ValueSchema::parse(std::string_view str) {
    return ValueSchemaParser::parse(str);
}

size_t ValueSchema::hash() const {
    auto baseHash = static_cast<size_t>(_type);

    visitNestedType(
        *this,
        [&](const ValueSchemaTypeReference& typeReference) { boost::hash_combine(baseHash, typeReference.hash()); },
        [&](const GenericValueSchemaTypeReference& genericTypeReference) {
            boost::hash_combine(baseHash, genericTypeReference.getType().hash());
            for (size_t i = 0; i < genericTypeReference.getTypeArgumentsSize(); i++) {
                boost::hash_combine(baseHash, genericTypeReference.getTypeArgument(i).hash());
            }
        },
        [&](const ClassSchema& classSchema) { boost::hash_combine(baseHash, classSchema.hash()); },
        [&](const EnumSchema& enumSchema) {
            boost::hash_combine(baseHash, enumSchema.getName().hash());
            boost::hash_combine(baseHash, enumSchema.getCaseSchema().hash());
            for (size_t i = 0; i < enumSchema.getCasesSize(); i++) {
                const auto& enumCase = enumSchema.getCase(i);
                boost::hash_combine(baseHash, enumCase.name.hash());
                boost::hash_combine(baseHash, enumCase.value.hash());
            }
        },
        [&](const ValueFunctionSchema& functionSchema) {
            const auto& attributes = functionSchema.getAttributes();
            boost::hash_combine(baseHash, attributes.isMethod());
            boost::hash_combine(baseHash, attributes.isSingleCall());
            boost::hash_combine(baseHash, attributes.shouldDispatchToWorkerThread());
            boost::hash_combine(baseHash, attributes.allowSyncCall());
            boost::hash_combine(baseHash, functionSchema.getReturnValue().hash());
            for (size_t i = 0; i < functionSchema.getParametersSize(); i++) {
                boost::hash_combine(baseHash, functionSchema.getParameter(i).hash());
            }
        },
        [&](const ArraySchema& arraySchema) { boost::hash_combine(baseHash, arraySchema.getElementSchema().hash()); },
        [&](const MapSchema& mapSchema) {
            boost::hash_combine(baseHash, mapSchema.getKey().hash());
            boost::hash_combine(baseHash, mapSchema.getValue().hash());
        },
        [&](const ES6MapSchema& es6mapSchema) {
            boost::hash_combine(baseHash, es6mapSchema.getKey().hash());
            boost::hash_combine(baseHash, es6mapSchema.getValue().hash());
        },
        [&](const ES6SetSchema& es6setSchema) { boost::hash_combine(baseHash, es6setSchema.getKey().hash()); },
        [&](const OutcomeSchema& outcomeSchema) {
            boost::hash_combine(baseHash, outcomeSchema.getValue().hash());
            boost::hash_combine(baseHash, outcomeSchema.getError().hash());
        },
        [&](const ProtoSchema& protoSchema) { boost::hash_combine(baseHash, protoSchema.hash()); },
        [&](const PromiseSchema& promiseSchema) {
            boost::hash_combine(baseHash, promiseSchema.getValueSchema().hash());
        },
        [&](const ValueSchemaReference& schemaReference) {
            boost::hash_combine(baseHash, schemaReference.getKey().hash());
        });

    boost::hash_combine(baseHash, _flags);
    return baseHash;
}

ValueSchema ValueSchema::asOptional() const {
    return ValueSchema::optional(*this);
}

ValueSchema ValueSchema::asNonOptional() const {
    return ValueSchema::nonOptional(*this);
}

ValueSchema ValueSchema::asBoxed() const {
    return ValueSchema::boxed(*this);
}

ValueSchema ValueSchema::asArray() const {
    return ValueSchema::array(*this);
}

bool ValueSchema::isUntyped() const {
    return _type == ValueSchemaInnerTypeUntyped;
}

bool ValueSchema::isVoid() const {
    return _type == ValueSchemaInnerTypeVoid;
}

bool ValueSchema::isOptional() const {
    return (_flags & kOptionalFlagBit) != 0;
}

bool ValueSchema::isBoxed() const {
    return (_flags & kBoxedFlagBit) != 0;
}

bool ValueSchema::isString() const {
    return _type == ValueSchemaInnerTypeString;
}

bool ValueSchema::isInteger() const {
    return _type == ValueSchemaInnerTypeInt;
}

bool ValueSchema::isLongInteger() const {
    return _type == ValueSchemaInnerTypeLong;
}

bool ValueSchema::isDouble() const {
    return _type == ValueSchemaInnerTypeDouble;
}

bool ValueSchema::isBoolean() const {
    return _type == ValueSchemaInnerTypeBoolean;
}

bool ValueSchema::isFunction() const {
    return _type == ValueSchemaInnerTypeFunction;
}

bool ValueSchema::isClass() const {
    return _type == ValueSchemaInnerTypeClass;
}

bool ValueSchema::isEnum() const {
    return _type == ValueSchemaInnerTypeEnum;
}

bool ValueSchema::isValueTypedArray() const {
    return _type == ValueSchemaInnerTypeValueTypedArray;
}

bool ValueSchema::isArray() const {
    return _type == ValueSchemaInnerTypeArray;
}

bool ValueSchema::isMap() const {
    return _type == ValueSchemaInnerTypeMap;
}

bool ValueSchema::isES6Map() const {
    return _type == ValueSchemaInnerTypeES6Map;
}

bool ValueSchema::isES6Set() const {
    return _type == ValueSchemaInnerTypeES6Set;
}

bool ValueSchema::isOutcome() const {
    return _type == ValueSchemaInnerTypeOutcome;
}

bool ValueSchema::isDate() const {
    return _type == ValueSchemaInnerTypeDate;
}

bool ValueSchema::isProto() const {
    return _type == ValueSchemaInnerTypeProto;
}

bool ValueSchema::isPromise() const {
    return _type == ValueSchemaInnerTypePromise;
}

bool ValueSchema::isTypeReference() const {
    return _type == ValueSchemaInnerTypeNamedTypeReference || _type == ValueSchemaInnerTypePositionalTypeReference;
}

bool ValueSchema::isGenericTypeReference() const {
    return _type == ValueSchemaInnerTypeGenericTypeReference;
}

bool ValueSchema::isSchemaReference() const {
    return _type == ValueSchemaInnerTypeSchemaReference;
}

const ValueFunctionSchema* ValueSchema::getFunction() const {
    return dynamic_cast<const ValueFunctionSchema*>(_ptr.get());
}

Ref<ValueFunctionSchema> ValueSchema::getFunctionRef() const {
    return castOrNull<ValueFunctionSchema>(_ptr);
}

const ClassSchema* ValueSchema::getClass() const {
    return dynamic_cast<const ClassSchema*>(_ptr.get());
}

Ref<ClassSchema> ValueSchema::getClassRef() const {
    return castOrNull<ClassSchema>(_ptr);
}

const EnumSchema* ValueSchema::getEnum() const {
    return dynamic_cast<const EnumSchema*>(_ptr.get());
}

Ref<EnumSchema> ValueSchema::getEnumRef() const {
    return castOrNull<EnumSchema>(_ptr);
}

const ArraySchema* ValueSchema::getArray() const {
    return dynamic_cast<const ArraySchema*>(_ptr.get());
}

const MapSchema* ValueSchema::getMap() const {
    return dynamic_cast<const MapSchema*>(_ptr.get());
}

const ES6MapSchema* ValueSchema::getES6Map() const {
    return dynamic_cast<const ES6MapSchema*>(_ptr.get());
}

const ES6SetSchema* ValueSchema::getES6Set() const {
    return dynamic_cast<const ES6SetSchema*>(_ptr.get());
}

const OutcomeSchema* ValueSchema::getOutcome() const {
    return dynamic_cast<const OutcomeSchema*>(_ptr.get());
}

const ProtoSchema* ValueSchema::getProto() const {
    return dynamic_cast<const ProtoSchema*>(_ptr.get());
}

const PromiseSchema* ValueSchema::getPromise() const {
    return dynamic_cast<const PromiseSchema*>(_ptr.get());
}

const ValueSchemaReference* ValueSchema::getSchemaReference() const {
    return dynamic_cast<const ValueSchemaReference*>(_ptr.get());
}

ValueSchemaTypeReference ValueSchema::getTypeReference() const {
    if (_type == ValueSchemaInnerTypeNamedTypeReference) {
        return ValueSchemaTypeReference::named(static_cast<ValueSchemaTypeReferenceTypeHint>(_typeReferenceFlags),
                                               StringBox(castOrNull<InternedStringImpl>(_ptr)));
    } else if (_type == ValueSchemaInnerTypePositionalTypeReference) {
        return ValueSchemaTypeReference::positional(
            static_cast<ValueSchemaTypeReference::Position>(_typeReferenceFlags));
    }
    return ValueSchemaTypeReference();
}

const GenericValueSchemaTypeReference* ValueSchema::getGenericTypeReference() const {
    return dynamic_cast<const GenericValueSchemaTypeReference*>(_ptr.get());
}

ValueSchema ValueSchema::getArrayItemSchema() const {
    const auto* araraySchema = getArray();
    if (araraySchema == nullptr) {
        return ValueSchema::voidType();
    }

    return araraySchema->getElementSchema();
}

ValueSchema ValueSchema::getReferencedSchema() const {
    const auto* schemaReference = getSchemaReference();
    if (schemaReference == nullptr) {
        return ValueSchema::voidType();
    }

    return schemaReference->getSchema();
}

ValueSchema& ValueSchema::operator=(const ValueSchema& other) {
    if (this != &other) {
        _type = other._type;
        _flags = other._flags;
        _typeReferenceFlags = other._typeReferenceFlags;
        _ptr = other._ptr;

        sanityCheck();
    }
    return *this;
}

ValueSchema& ValueSchema::operator=(ValueSchema&& other) noexcept {
    if (this != &other) {
        _type = other._type;
        _flags = other._flags;
        _typeReferenceFlags = other._typeReferenceFlags;
        _ptr = std::move(other._ptr);

        other._type = ValueSchemaInnerTypeUntyped;
        other._flags = 0;
        other._typeReferenceFlags = 0;

        sanityCheck();
    }
    return *this;
}

bool ValueSchema::operator==(const ValueSchema& other) const {
    if (_type != other._type || _flags != other._flags) {
        return false;
    }

    if (_ptr != nullptr && _ptr == other._ptr) {
        // Fast path for nested types when ptr are equals
        return _typeReferenceFlags == other._typeReferenceFlags;
    }

    bool isEqual = true;

    visitNestedType(
        *this,
        [&](const ValueSchemaTypeReference& typeReference) { isEqual = typeReference == other.getTypeReference(); },
        [&](const GenericValueSchemaTypeReference& genericTypeReference) {
            isEqual = genericTypeReference == *other.getGenericTypeReference();
        },
        [&](const ClassSchema& classSchema) { isEqual = classSchema == *other.getClass(); },
        [&](const EnumSchema& enumSchema) { isEqual = enumSchema == *other.getEnum(); },
        [&](const ValueFunctionSchema& functionSchema) { isEqual = functionSchema == *other.getFunction(); },
        [&](const ArraySchema& arraySchema) { isEqual = arraySchema.getElementSchema() == other.getArrayItemSchema(); },
        [&](const MapSchema& mapSchema) {
            const auto* otherMapSchema = other.getMap();
            isEqual = (mapSchema.getKey() == otherMapSchema->getKey()) &&
                      (mapSchema.getValue() == otherMapSchema->getValue());
        },
        [&](const ES6MapSchema& mapSchema) {
            const auto* otherMapSchema = other.getES6Map();
            isEqual = (mapSchema.getKey() == otherMapSchema->getKey()) &&
                      (mapSchema.getValue() == otherMapSchema->getValue());
        },
        [&](const ES6SetSchema& setSchema) {
            const auto* otherSetSchema = other.getES6Set();
            isEqual = (setSchema.getKey() == otherSetSchema->getKey());
        },
        [&](const OutcomeSchema& outcomeSchema) {
            const auto* otherOutcomeSchema = other.getOutcome();
            isEqual = (outcomeSchema.getValue() == otherOutcomeSchema->getValue()) &&
                      (outcomeSchema.getError() == otherOutcomeSchema->getError());
        },
        [&](const ProtoSchema& protoSchema) { isEqual = protoSchema == *other.getProto(); },
        [&](const PromiseSchema& promiseSchema) {
            isEqual = promiseSchema.getValueSchema() == other.getPromise()->getValueSchema();
        },
        [&](const ValueSchemaReference& schemaReference) {
            // Compare just reference equality for schema references
            isEqual = &schemaReference == other.getSchemaReference();
        });

    return isEqual;
}

bool ValueSchema::operator!=(const ValueSchema& other) const {
    return !(*this == other);
}

ValueSchema ValueSchema::untyped() {
    return ValueSchema(ValueSchemaInnerTypeUntyped, 0, 0, nullptr);
}

ValueSchema ValueSchema::voidType() {
    return ValueSchema(ValueSchemaInnerTypeVoid, 0, 0, nullptr);
}

ValueSchema ValueSchema::integer() {
    return ValueSchema(ValueSchemaInnerTypeInt, 0, 0, nullptr);
}

ValueSchema ValueSchema::longInteger() {
    return ValueSchema(ValueSchemaInnerTypeLong, 0, 0, nullptr);
}

ValueSchema ValueSchema::doublePrecision() {
    return ValueSchema(ValueSchemaInnerTypeDouble, 0, 0, nullptr);
}

ValueSchema ValueSchema::boolean() {
    return ValueSchema(ValueSchemaInnerTypeBoolean, 0, 0, nullptr);
}

ValueSchema ValueSchema::string() {
    return ValueSchema(ValueSchemaInnerTypeString, 0, 0, nullptr);
}

ValueSchema ValueSchema::valueTypedArray() {
    return ValueSchema(ValueSchemaInnerTypeValueTypedArray, 0, 0, nullptr);
}

ValueSchema ValueSchema::date() {
    return ValueSchema(ValueSchemaInnerTypeDate, 0, 0, nullptr);
}

ValueSchema ValueSchema::typeReference(const ValueSchemaTypeReference& reference) {
    if (reference.isPositional()) {
        return ValueSchema(ValueSchemaInnerTypePositionalTypeReference, 0, reference.getPosition(), nullptr);
    } else {
        return ValueSchema(ValueSchemaInnerTypeNamedTypeReference,
                           0,
                           reference.getTypeHint(),
                           reference.getName().getInternedString());
    }
}

ValueSchema ValueSchema::genericTypeReference(const ValueSchemaTypeReference& type,
                                              const ValueSchema* typeArguments,
                                              size_t typeArgumentsSize) {
    auto genericTypeReference = GenericValueSchemaTypeReference::make(type, typeArgumentsSize);
    for (size_t i = 0; i < typeArgumentsSize; i++) {
        genericTypeReference->getTypeArgument(i) = typeArguments[i];
    }
    return ValueSchema(ValueSchemaInnerTypeGenericTypeReference, 0, 0, genericTypeReference);
}

ValueSchema ValueSchema::cls(const StringBox& className,
                             bool isInterface,
                             const ClassPropertySchema* properties,
                             size_t propertiesSize) {
    auto classSchema = ClassSchema::make(className, isInterface, propertiesSize);
    for (size_t i = 0; i < propertiesSize; i++) {
        classSchema->getProperty(i) = properties[i];
    }

    return ValueSchema::cls(classSchema);
}

ValueSchema ValueSchema::cls(const Ref<ClassSchema>& classSchema) {
    return ValueSchema(ValueSchemaInnerTypeClass, 0, 0, classSchema);
}

ValueSchema ValueSchema::enumeration(const StringBox& name,
                                     const ValueSchema& caseSchema,
                                     const EnumCaseSchema* cases,
                                     size_t casesSize) {
    auto enumSchema = EnumSchema::make(name, caseSchema, casesSize);
    for (size_t i = 0; i < casesSize; i++) {
        enumSchema->getCase(i) = cases[i];
    }

    return ValueSchema(ValueSchemaInnerTypeEnum, 0, 0, enumSchema);
}

ValueSchema ValueSchema::function(const ValueFunctionSchemaAttributes& attributes,
                                  const ValueSchema& returnValue,
                                  const ValueSchema* parameters,
                                  size_t parametersSize) {
    auto valueFunctionSchema = ValueFunctionSchema::make(attributes, returnValue, parametersSize);
    for (size_t i = 0; i < parametersSize; i++) {
        valueFunctionSchema->getParameter(i) = parameters[i];
    }

    return ValueSchema(ValueSchemaInnerTypeFunction, 0, 0, valueFunctionSchema);
}

ValueSchema ValueSchema::array(const ValueSchema& itemSchema) {
    return ValueSchema(ValueSchemaInnerTypeArray, 0, 0, makeShared<ArraySchema>(itemSchema));
}

ValueSchema ValueSchema::array(ValueSchema&& itemSchema) {
    return ValueSchema(ValueSchemaInnerTypeArray, 0, 0, makeShared<ArraySchema>(std::move(itemSchema)));
}

ValueSchema ValueSchema::map(const ValueSchema& keySchema, const ValueSchema& valueSchema) {
    return ValueSchema(ValueSchemaInnerTypeMap, 0, 0, makeShared<MapSchema>(keySchema, valueSchema));
}

ValueSchema ValueSchema::map(ValueSchema&& keySchema, ValueSchema&& valueSchema) {
    return ValueSchema(
        ValueSchemaInnerTypeMap, 0, 0, makeShared<MapSchema>(std::move(keySchema), std::move(valueSchema)));
}

ValueSchema ValueSchema::promise(ValueSchema&& valueSchema) {
    return ValueSchema(ValueSchemaInnerTypePromise, 0, 0, makeShared<PromiseSchema>(std::move(valueSchema)));
}

ValueSchema ValueSchema::promise(const ValueSchema& valueSchema) {
    return ValueSchema(ValueSchemaInnerTypePromise, 0, 0, makeShared<PromiseSchema>(valueSchema));
}

ValueSchema ValueSchema::optional(const ValueSchema& itemSchema) {
    return ValueSchema(
        itemSchema._type, itemSchema._flags | kOptionalFlagBit, itemSchema._typeReferenceFlags, itemSchema._ptr);
}

ValueSchema ValueSchema::nonOptional(const ValueSchema& itemSchema) {
    return ValueSchema(
        itemSchema._type, itemSchema._flags & ~kOptionalFlagBit, itemSchema._typeReferenceFlags, itemSchema._ptr);
}

ValueSchema ValueSchema::optional(ValueSchema&& itemSchema) {
    auto out = ValueSchema(std::move(itemSchema));
    out._flags |= kOptionalFlagBit;

    return out;
}

ValueSchema ValueSchema::boxed(const ValueSchema& itemSchema) {
    return ValueSchema(
        itemSchema._type, itemSchema._flags | kBoxedFlagBit, itemSchema._typeReferenceFlags, itemSchema._ptr);
}

ValueSchema ValueSchema::boxed(ValueSchema&& itemSchema) {
    auto out = ValueSchema(std::move(itemSchema));
    out._flags |= kBoxedFlagBit;

    return out;
}

ValueSchema ValueSchema::schemaReference(const Ref<ValueSchemaReference>& schemaReference) {
    return ValueSchema(ValueSchemaInnerTypeSchemaReference, 0, 0, schemaReference);
}

ValueSchema ValueSchema::es6map(const ValueSchema& keySchema, const ValueSchema& valueSchema) {
    return ValueSchema(ValueSchemaInnerTypeES6Map, 0, 0, makeShared<ES6MapSchema>(keySchema, valueSchema));
}

ValueSchema ValueSchema::es6set(const ValueSchema& keySchema) {
    return ValueSchema(ValueSchemaInnerTypeES6Set, 0, 0, makeShared<ES6SetSchema>(keySchema));
}

ValueSchema ValueSchema::outcome(const ValueSchema& valueSchema, const ValueSchema& errorSchema) {
    return ValueSchema(ValueSchemaInnerTypeOutcome, 0, 0, makeShared<OutcomeSchema>(valueSchema, errorSchema));
}

ValueSchema ValueSchema::proto(const std::string_view* classNamePartsBegin, const std::string_view* classNamePartsEnd) {
    return ValueSchema(
        ValueSchemaInnerTypeProto, 0, 0, makeShared<ProtoSchema>(classNamePartsBegin, classNamePartsEnd));
}

static void checkNotNull(const void* ptr) {
    SC_ABORT_UNLESS(ptr != nullptr, "Nested type is null");
}

void ValueSchema::sanityCheck() const {
    // Ticket: 1974
    // Temporary check to try to figure out the root cause of what's happening
    // in the JIRA above. It seems like the _ptr can become null while the type
    // is set to something that expects a valid ptr.

    visitNestedType(
        *this,
        [&](const ValueSchemaTypeReference& typeReference) { checkNotNull(&typeReference); },
        [&](const GenericValueSchemaTypeReference& genericTypeReference) { checkNotNull(&genericTypeReference); },
        [&](const ClassSchema& classSchema) { checkNotNull(&classSchema); },
        [&](const EnumSchema& enumSchema) { checkNotNull(&enumSchema); },
        [&](const ValueFunctionSchema& functionSchema) { checkNotNull(&functionSchema); },
        [&](const ArraySchema& arraySchema) { checkNotNull(&arraySchema); },
        [&](const MapSchema& mapSchema) { checkNotNull(&mapSchema); },
        [&](const ES6MapSchema& mapSchema) { checkNotNull(&mapSchema); },
        [&](const ES6SetSchema& setSchema) { checkNotNull(&setSchema); },
        [&](const OutcomeSchema& outcomeSchema) { checkNotNull(&outcomeSchema); },
        [&](const ProtoSchema& protoSchema) { checkNotNull(&protoSchema); },
        [&](const PromiseSchema& promiseSchema) { checkNotNull(&promiseSchema); },
        [&](const ValueSchemaReference& schemaReference) { checkNotNull(&schemaReference); });
}

ArraySchema::ArraySchema(ValueSchema&& elementSchema) : _elementSchema(std::move(elementSchema)) {}

ArraySchema::ArraySchema(const ValueSchema& elementSchema) : _elementSchema(elementSchema) {}
ArraySchema::~ArraySchema() = default;

const ValueSchema& ArraySchema::getElementSchema() const {
    return _elementSchema;
}

MapSchema::MapSchema(ValueSchema&& key, ValueSchema&& value) : _key(std::move(key)), _value(std::move(value)) {}
MapSchema::MapSchema(const ValueSchema& key, const ValueSchema& value) : _key(key), _value(value) {}
MapSchema::~MapSchema() = default;

const ValueSchema& MapSchema::getKey() const {
    return _key;
}

const ValueSchema& MapSchema::getValue() const {
    return _value;
}

ES6MapSchema::ES6MapSchema(ValueSchema&& key, ValueSchema&& value) : _key(std::move(key)), _value(std::move(value)) {}
ES6MapSchema::ES6MapSchema(const ValueSchema& key, const ValueSchema& value) : _key(key), _value(value) {}
ES6MapSchema::~ES6MapSchema() = default;

const ValueSchema& ES6MapSchema::getKey() const {
    return _key;
}

const ValueSchema& ES6MapSchema::getValue() const {
    return _value;
}

ES6SetSchema::ES6SetSchema(ValueSchema&& key) : _key(std::move(key)) {}
ES6SetSchema::ES6SetSchema(const ValueSchema& key) : _key(key) {}
ES6SetSchema::~ES6SetSchema() = default;

const ValueSchema& ES6SetSchema::getKey() const {
    return _key;
}

OutcomeSchema::OutcomeSchema(ValueSchema&& valueSchema, ValueSchema&& errorSchema)
    : _value(std::move(valueSchema)), _error(std::move(errorSchema)) {}
OutcomeSchema::OutcomeSchema(const ValueSchema& valueSchema, const ValueSchema& errorSchema)
    : _value(valueSchema), _error(errorSchema) {}
OutcomeSchema::~OutcomeSchema() = default;

const ValueSchema& OutcomeSchema::getValue() const {
    return _value;
}

const ValueSchema& OutcomeSchema::getError() const {
    return _error;
}

ProtoSchema::ProtoSchema(const std::string_view* classNamePartsBegin, const std::string_view* classNamePartsEnd)
    : _classNameParts(classNamePartsBegin, classNamePartsEnd) {}
ProtoSchema::~ProtoSchema() = default;

size_t ProtoSchema::hash() const {
    size_t baseHash = 0;
    for (const auto& part : _classNameParts) {
        boost::hash_combine(baseHash, reinterpret_cast<size_t>(part.data()));
    }
    return baseHash;
}

bool ProtoSchema::operator==(const ProtoSchema& other) const {
    if (_classNameParts.size() != other._classNameParts.size()) {
        return false;
    }
    for (size_t i = 0; i < _classNameParts.size(); ++i) {
        if (_classNameParts[i] != other._classNameParts[i]) {
            return false;
        }
    }
    return true;
}

bool ProtoSchema::operator!=(const ProtoSchema& other) const {
    return !(*this == other);
}

PromiseSchema::PromiseSchema(ValueSchema&& valueSchema) : _valueSchema(std::move(valueSchema)) {}
PromiseSchema::PromiseSchema(const ValueSchema& valueSchema) : _valueSchema(valueSchema) {}
PromiseSchema::~PromiseSchema() = default;

const ValueSchema& PromiseSchema::getValueSchema() const {
    return _valueSchema;
}

ClassSchema::~ClassSchema() {
    InlineContainerAllocator<ClassSchema, ClassPropertySchema> allocator;
    allocator.deallocate(this, _propertiesSize);
}

Ref<ClassSchema> ClassSchema::make(const StringBox& className, bool isInterface, size_t propertiesSize) {
    InlineContainerAllocator<ClassSchema, ClassPropertySchema> allocator;
    return allocator.allocate(propertiesSize, className, isInterface, propertiesSize);
}

ClassSchema::ClassSchema(const StringBox& className, bool isInterface, size_t propertiesSize)
    : _className(className), _isInterface(isInterface), _propertiesSize(propertiesSize) {
    InlineContainerAllocator<ClassSchema, ClassPropertySchema> allocator;
    allocator.constructContainerEntries(this, propertiesSize);
}

const StringBox& ClassSchema::getClassName() const {
    return _className;
}

bool ClassSchema::isInterface() const {
    return _isInterface;
}

size_t ClassSchema::getPropertiesSize() const {
    return _propertiesSize;
}

const ClassPropertySchema* ClassSchema::getProperties() const {
    InlineContainerAllocator<ClassSchema, ClassPropertySchema> allocator;
    return allocator.getContainerStartPtr(this);
}

const ClassPropertySchema& ClassSchema::getProperty(size_t index) const {
    return getProperties()[index];
}

ClassPropertySchema& ClassSchema::getProperty(size_t index) {
    InlineContainerAllocator<ClassSchema, ClassPropertySchema> allocator;
    return allocator.getContainerStartPtr(this)[index];
}

const ClassPropertySchema* ClassSchema::begin() const {
    InlineContainerAllocator<ClassSchema, ClassPropertySchema> allocator;
    return allocator.getContainerStartPtr(this);
}

const ClassPropertySchema* ClassSchema::end() const {
    InlineContainerAllocator<ClassSchema, ClassPropertySchema> allocator;
    return &allocator.getContainerStartPtr(this)[_propertiesSize];
}

std::optional<size_t> ClassSchema::getPropertyIndexForName(const StringBox& propertyName) const {
    // TODO(simon): Could consider using a map if needed to speed up the query.
    size_t index = 0;
    for (const auto& property : *this) {
        if (property.name == propertyName) {
            return index;
        }
        index++;
    }
    return std::nullopt;
}

size_t ClassSchema::hash() const {
    size_t baseHash = 0;
    boost::hash_combine(baseHash, getClassName().hash());
    for (const auto& property : *this) {
        boost::hash_combine(baseHash, property.name.hash());
        boost::hash_combine(baseHash, property.schema.hash());
    }

    return baseHash;
}

bool ClassSchema::operator==(const ClassSchema& other) const {
    if (_className != other._className || _isInterface != other._isInterface ||
        _propertiesSize != other._propertiesSize) {
        return false;
    }

    for (size_t i = 0; i < _propertiesSize; i++) {
        const auto& leftProperty = getProperty(i);
        const auto& rightProperty = other.getProperty(i);
        if (leftProperty.name != rightProperty.name || leftProperty.schema != rightProperty.schema) {
            return false;
        }
    }

    return true;
}

bool ClassSchema::operator!=(const ClassSchema& other) const {
    return !(*this == other);
}

EnumSchema::EnumSchema(const StringBox& name, const ValueSchema& caseSchema, size_t casesSize)
    : _name(name), _caseSchema(caseSchema), _casesSize(casesSize) {
    InlineContainerAllocator<EnumSchema, EnumCaseSchema> allocator;
    allocator.constructContainerEntries(this, casesSize);
}

EnumSchema::~EnumSchema() {
    InlineContainerAllocator<EnumSchema, EnumCaseSchema> allocator;
    allocator.deallocate(this, _casesSize);
}

const StringBox& EnumSchema::getName() const {
    return _name;
}

const ValueSchema& EnumSchema::getCaseSchema() const {
    return _caseSchema;
}

size_t EnumSchema::getCasesSize() const {
    return _casesSize;
}

const EnumCaseSchema& EnumSchema::getCase(size_t index) const {
    InlineContainerAllocator<EnumSchema, EnumCaseSchema> allocator;
    return allocator.getContainerStartPtr(this)[index];
}

EnumCaseSchema& EnumSchema::getCase(size_t index) {
    InlineContainerAllocator<EnumSchema, EnumCaseSchema> allocator;
    return allocator.getContainerStartPtr(this)[index];
}

const EnumCaseSchema* EnumSchema::begin() const {
    InlineContainerAllocator<EnumSchema, EnumCaseSchema> allocator;
    return allocator.getContainerStartPtr(this);
}

const EnumCaseSchema* EnumSchema::end() const {
    InlineContainerAllocator<EnumSchema, EnumCaseSchema> allocator;
    return &allocator.getContainerStartPtr(this)[_casesSize];
}

bool EnumSchema::operator==(const EnumSchema& other) const {
    if (_name != other._name || _caseSchema != other._caseSchema || _casesSize != other._casesSize) {
        return false;
    }

    for (size_t i = 0; i < _casesSize; i++) {
        const auto& leftCaseSchema = getCase(i);
        const auto& rightCaseSchema = other.getCase(i);
        if (leftCaseSchema.name != rightCaseSchema.name || leftCaseSchema.value != rightCaseSchema.value) {
            return false;
        }
    }

    return true;
}

bool EnumSchema::operator!=(const EnumSchema& other) const {
    return !(*this == other);
}

Ref<EnumSchema> EnumSchema::make(const StringBox& name, const ValueSchema& caseSchema, size_t casesSize) {
    InlineContainerAllocator<EnumSchema, EnumCaseSchema> allocator;
    return allocator.allocate(casesSize, name, caseSchema, casesSize);
}

ValueFunctionSchemaAttributes::ValueFunctionSchemaAttributes() = default;

ValueFunctionSchemaAttributes::ValueFunctionSchemaAttributes(bool isMethod,
                                                             bool isSingleCall,
                                                             bool shouldDispatchToWorkerThread)
    : _isMethod(isMethod), _isSingleCall(isSingleCall), _shouldDispatchToWorkerThread(shouldDispatchToWorkerThread) {}

ValueFunctionSchemaAttributes::ValueFunctionSchemaAttributes(bool isMethod,
                                                             bool isSingleCall,
                                                             bool shouldDispatchToWorkerThread,
                                                             bool allowSyncCall)
    : _isMethod(isMethod),
      _isSingleCall(isSingleCall),
      _shouldDispatchToWorkerThread(shouldDispatchToWorkerThread),
      _allowSyncCall(allowSyncCall) {}

bool ValueFunctionSchemaAttributes::empty() const {
    // Show modifiers block when we have any; bansync (`b`) is emitted when allowSyncCall is false
    return !(_isMethod || _isSingleCall || _shouldDispatchToWorkerThread || !_allowSyncCall);
}

bool ValueFunctionSchemaAttributes::isMethod() const {
    return _isMethod;
}

bool ValueFunctionSchemaAttributes::isSingleCall() const {
    return _isSingleCall;
}

bool ValueFunctionSchemaAttributes::shouldDispatchToWorkerThread() const {
    return _shouldDispatchToWorkerThread;
}

bool ValueFunctionSchemaAttributes::allowSyncCall() const {
    return _allowSyncCall;
}

bool ValueFunctionSchemaAttributes::operator==(const ValueFunctionSchemaAttributes& other) const {
    return _isMethod == other._isMethod && _isSingleCall == other._isSingleCall &&
           _shouldDispatchToWorkerThread == other._shouldDispatchToWorkerThread &&
           _allowSyncCall == other._allowSyncCall;
}

bool ValueFunctionSchemaAttributes::operator!=(const ValueFunctionSchemaAttributes& other) const {
    return !(*this == other);
}

ValueFunctionSchema::ValueFunctionSchema(const ValueFunctionSchemaAttributes& attributes,
                                         const ValueSchema& returnValue,
                                         size_t parametersSize)
    : _attributes(attributes), _returnValue(returnValue), _parametersSize(parametersSize) {
    InlineContainerAllocator<ValueFunctionSchema, ValueSchema> allocator;
    allocator.constructContainerEntries(this, parametersSize);
}

ValueFunctionSchema::~ValueFunctionSchema() {
    InlineContainerAllocator<ValueFunctionSchema, ValueSchema> allocator;
    allocator.deallocate(this, _parametersSize);
}

const ValueSchema& ValueFunctionSchema::getReturnValue() const {
    return _returnValue;
}

ValueSchema& ValueFunctionSchema::getReturnValue() {
    return _returnValue;
}

size_t ValueFunctionSchema::getParametersSize() const {
    return _parametersSize;
}

const ValueFunctionSchemaAttributes& ValueFunctionSchema::getAttributes() const {
    return _attributes;
}

bool ValueFunctionSchema::operator==(const ValueFunctionSchema& other) const {
    if (_returnValue != other._returnValue || _parametersSize != other._parametersSize ||
        _attributes != other._attributes) {
        return false;
    }

    for (size_t i = 0; i < _parametersSize; i++) {
        const auto& leftParameter = getParameter(i);
        const auto& rightParameter = other.getParameter(i);
        if (leftParameter != rightParameter) {
            return false;
        }
    }

    return true;
}

bool ValueFunctionSchema::operator!=(const ValueFunctionSchema& other) const {
    return !(*this == other);
}

const ValueSchema* ValueFunctionSchema::getParameters() const {
    InlineContainerAllocator<ValueFunctionSchema, ValueSchema> allocator;
    return allocator.getContainerStartPtr(this);
}

const ValueSchema& ValueFunctionSchema::getParameter(size_t index) const {
    return getParameters()[index];
}

ValueSchema& ValueFunctionSchema::getParameter(size_t index) {
    InlineContainerAllocator<ValueFunctionSchema, ValueSchema> allocator;
    return allocator.getContainerStartPtr(this)[index];
}

Ref<ValueFunctionSchema> ValueFunctionSchema::make(const ValueFunctionSchemaAttributes& attributes,
                                                   const ValueSchema& returnValue,
                                                   size_t parametersSize) {
    InlineContainerAllocator<ValueFunctionSchema, ValueSchema> allocator;
    return allocator.allocate(parametersSize, attributes, returnValue, parametersSize);
}

GenericValueSchemaTypeReference::GenericValueSchemaTypeReference(const ValueSchemaTypeReference& type,
                                                                 size_t typeArgumentsSize)
    : _type(type), _typeArgumentsSize(typeArgumentsSize) {
    InlineContainerAllocator<GenericValueSchemaTypeReference, ValueSchema> allocator;
    allocator.constructContainerEntries(this, typeArgumentsSize);
}

GenericValueSchemaTypeReference::~GenericValueSchemaTypeReference() {
    InlineContainerAllocator<GenericValueSchemaTypeReference, ValueSchema> allocator;
    allocator.deallocate(this, _typeArgumentsSize);
}

const ValueSchemaTypeReference& GenericValueSchemaTypeReference::getType() const {
    return _type;
}

size_t GenericValueSchemaTypeReference::getTypeArgumentsSize() const {
    return _typeArgumentsSize;
}

const ValueSchema& GenericValueSchemaTypeReference::getTypeArgument(size_t index) const {
    InlineContainerAllocator<GenericValueSchemaTypeReference, ValueSchema> allocator;
    return allocator.getContainerStartPtr(this)[index];
}

ValueSchema& GenericValueSchemaTypeReference::getTypeArgument(size_t index) {
    InlineContainerAllocator<GenericValueSchemaTypeReference, ValueSchema> allocator;
    return allocator.getContainerStartPtr(this)[index];
}

bool GenericValueSchemaTypeReference::operator==(const GenericValueSchemaTypeReference& other) const {
    if (_type != other._type || _typeArgumentsSize != other._typeArgumentsSize) {
        return false;
    }

    for (size_t i = 0; i < _typeArgumentsSize; i++) {
        if (getTypeArgument(i) != other.getTypeArgument(i)) {
            return false;
        }
    }

    return true;
}

bool GenericValueSchemaTypeReference::operator!=(const GenericValueSchemaTypeReference& other) const {
    return !(*this == other);
}

Ref<GenericValueSchemaTypeReference> GenericValueSchemaTypeReference::make(const ValueSchemaTypeReference& type,
                                                                           size_t typeArgumentsSize) {
    InlineContainerAllocator<GenericValueSchemaTypeReference, ValueSchema> allocator;
    return allocator.allocate(typeArgumentsSize, type, typeArgumentsSize);
}

std::ostream& operator<<(std::ostream& os, const ValueSchema& value) {
    return os << value.toString(ValueSchemaPrintFormat::HumanReadable, false);
}

} // namespace Valdi
