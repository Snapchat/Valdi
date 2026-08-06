//
//  PlatformValueDelegate.hpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 1/24/23.
//

#pragma once

#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include <mutex>

namespace Valdi {

class ReferenceInfoBuilder;
class Promise;
class IDispatchQueue;

enum class CollectionType { Map, Set };

template<typename ValueType>
class PlatformObjectStore {
public:
    std::recursive_mutex& mutex() const {
        return _mutex;
    }

    virtual ~PlatformObjectStore() = default;

    /**
     Given the object used as a key, return the associated value with it, or return nullptr otherwise
     */
    virtual Ref<RefCountable> getValueForObjectKey(const ValueType& objectKey, ExceptionTracker& exceptionTracker) = 0;

    /**
     Associate the given value with the given object used as a key.
     */
    virtual void setValueForObjectKey(const ValueType& objectKey,
                                      const Ref<RefCountable>& value,
                                      ExceptionTracker& exceptionTracker) = 0;

    /**
     Return the object associated with the given id.
     */
    virtual std::optional<ValueType> getObjectForId(uint32_t objectId, ExceptionTracker& exceptionTracker) = 0;

    /**
     Associate the given object with the given id. The object should be retained as a weak reference.
     */
    virtual void setObjectForId(uint32_t objectId, const ValueType& object, ExceptionTracker& exceptionTracker) = 0;

private:
    mutable std::recursive_mutex _mutex;
};

/**
 The PlatformObjectClassDelegate allows the ValueMarshallerRegistry
 to interact with a foreign platform object, like Objective-C, Kotlin or JS,
 of a specific class.
 */
template<typename ValueType>
class PlatformObjectClassDelegate : public SimpleRefCountable {
public:
    /**
     Return a new instance of the class represented by this class delegate,
     populated with the given property values.
     */
    virtual ValueType newObject(const ValueType* propertyValues, ExceptionTracker& exceptionTracker) = 0;

    /**
     Resolve the proeprty value for in the given object for the given property index.
     */
    virtual ValueType getProperty(const ValueType& object,
                                  size_t propertyIndex,
                                  ExceptionTracker& exceptionTracker) = 0;

    /**
     Create a new proxy reference from the given object.
     */
    virtual Ref<ValueTypedProxyObject> newProxy(const ValueType& object,
                                                const Ref<ValueTypedObject>& typedObject,
                                                ExceptionTracker& exceptionTracker) = 0;
};

/**
 The PlatformEnumClassDelegate allows the ValueMarshallerRegistry to interact with
 a foreign platform enum.
 */
template<typename ValueType>
class PlatformEnumClassDelegate : public SimpleRefCountable {
public:
    /**
     Return a instance instance of an enum for the given case index.
     If "asBoxed" is true, the returned instance should be in a boxed type,
     for platforms where boxing exists on enums like Objective-C.
     */
    virtual ValueType newEnum(size_t enumCaseIndex, bool asBoxed, ExceptionTracker& exceptionTracker) = 0;

    /**
     Given the enum case, retrieve the enum case value that it represents.
     */
    virtual Value enumCaseToValue(const ValueType& enumeration, bool isBoxed, ExceptionTracker& exceptionTracker) = 0;
};

/**
 The FunctionTrampoline is a helper that the bridged functions can use to
 forward call to a ValueFunction, or to unmarshall parameters from a Marshaller
 into a Platform ValueType and marshall the result back into the marshaller.
 It is used thus in both ways: when platform calls a function, or when a platform
 function is called from the ValueFunction API.
 */
template<typename ValueType>
class PlatformFunctionTrampoline : public SimpleRefCountable {
public:
    /**
     Forward a platform call into a ValueFunction.
     This method will take care of marshalling the parameters,
     calling the ValueFucntion, and unmarshalling the return value.
     The function will return an error type if the call should throw
     an error on the platform side.
     */
    virtual std::optional<ValueType> forwardCall(ValueFunction* valueFunction,
                                                 const ValueType* parameters,
                                                 size_t parametersSize,
                                                 const ReferenceInfoBuilder& referenceInfoBuilder,
                                                 const StringBox* precomputedFunctionIdentifier,
                                                 ExceptionTracker& exceptionTracker) = 0;

    /**
     Unmarshall parameters from the Marshaller into the given platform ValueType arrays.
     Return false on failure. This is used when a ValueFunction is calling a platform function,
     platform can use this function to convert into platform parameters.
     */
    virtual bool unmarshallParameters(const Value* inputParameters,
                                      size_t inputParametersSize,
                                      ValueType* outputParameters,
                                      size_t outputParametersSize,
                                      const ReferenceInfoBuilder& referenceInfoBuilder,
                                      ExceptionTracker& exceptionTracker) = 0;

    /**
     Marshall the return value provided as platform ValueType into the Marshaller. This is used when a platform
     function was just called from the ValueFunction API.
     */
    virtual Value marshallReturnValue(const ValueType& returnValue,
                                      const ReferenceInfoBuilder& referenceInfoBuilder,
                                      ExceptionTracker& exceptionTracker) = 0;

    /**
     Return the number of expected parameters.
     */
    virtual size_t getParametersSize() const = 0;

    /**
     Return the call queue that should be used to dispatch the call.
     */
    virtual const Ref<IDispatchQueue>& getCallQueue() const = 0;

    /**
     Return whether the return type of the function is expected to be a promise
     */
    virtual bool isPromiseReturnType() const = 0;
};

/**
 A PlatformFunctionClassDelegate allows the ValueMarshallerRegistry to
 interact with a platform function type or a specific type signature.
 */
template<typename ValueType>
class PlatformFunctionClassDelegate : public SimpleRefCountable {
public:
    /**
     Create a function that will call the given ValueFunction when the function is called from platform.
     The returned Value function should use the FunctionTrampoline previously provided when the Function class
     was created to help with parameters and return value marshalling.
     */
    virtual ValueType newFunction(const Ref<ValueFunction>& valueFunction,
                                  const ReferenceInfoBuilder& referenceInfoBuilder,
                                  ExceptionTracker& exceptionTracker) = 0;

    /**
     Create a ValueFunction that will call the given platform function. The returned ValueFunction should
     use the FunctionTrampoline previously provided when the Function class was created to help with
     parameters and return value marshalling.
     */
    virtual Ref<ValueFunction> toValueFunction(const ValueType* receiver,
                                               const ValueType& function,
                                               const ReferenceInfoBuilder& referenceInfoBuilder,
                                               ExceptionTracker& exceptionTracker) = 0;
};

template<typename ValueType>
class PlatformMapValueVisitor {
public:
    virtual ~PlatformMapValueVisitor() = default;

    /**
     Visit a property where the value is given as a platform value.
     */
    virtual bool visit(const ValueType& key, const ValueType& value, ExceptionTracker& exceptionTracker) = 0;

    /**
     Visit a property where the value is a given as a string.
     */
    virtual bool visit(const StringBox& key, const ValueType& value, ExceptionTracker& exceptionTracker) = 0;
};

template<typename ValueType>
class PlatformArrayBuilder {
public:
    PlatformArrayBuilder(ValueType&& builder) : _builder(std::move(builder)) {}

    const ValueType& getBuilder() const {
        return _builder;
    }

    ValueType& getBuilder() {
        return _builder;
    }

private:
    ValueType _builder;
};

template<typename ValueType>
class PlatformArrayIterator {
public:
    PlatformArrayIterator(ValueType&& iterator, size_t size) : _iterator(std::move(iterator)), _size(size) {}
    PlatformArrayIterator(const ValueType& iterator, size_t size) : _iterator(iterator), _size(size) {}

    const ValueType& getIterator() const {
        return _iterator;
    }

    size_t size() const {
        return _size;
    }

private:
    ValueType _iterator;
    size_t _size;
};

template<typename ValueType>
class ValueMarshaller;

/**
 PlatformValueDelegate is a generic interface which allows C++ to interact
 with values from a different platform. It can be used to allow C++ to create/manipulate
 Objective-C or Java values for instance.
 */
template<typename ValueType>
class PlatformValueDelegate : public SimpleRefCountable {
public:
    virtual ValueType newVoid() = 0;
    virtual ValueType newNull() = 0;

    virtual ValueType newInt(int32_t value, ExceptionTracker& exceptionTracker) = 0;
    virtual ValueType newIntObject(int32_t value, ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newLong(int64_t value, ExceptionTracker& exceptionTracker) = 0;
    virtual ValueType newLongObject(int64_t value, ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newDouble(double value, ExceptionTracker& exceptionTracker) = 0;
    virtual ValueType newDoubleObject(double value, ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newBool(bool value, ExceptionTracker& exceptionTracker) = 0;
    virtual ValueType newBoolObject(bool value, ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newStringUTF8(std::string_view str, ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newStringUTF16(std::u16string_view str, ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newString(const StaticString& str, ExceptionTracker& exceptionTracker) {
        switch (str.encoding()) {
            case StaticString::Encoding::UTF8:
                return newStringUTF8(str.utf8StringView(), exceptionTracker);
            case StaticString::Encoding::UTF16:
                return newStringUTF16(str.utf16StringView(), exceptionTracker);
            case StaticString::Encoding::UTF32: {
                auto utf8 = str.utf8Storage();
                return newStringUTF8(utf8.toStringView(), exceptionTracker);
            }
        }
    }

    virtual ValueType newByteArray(const BytesView& bytes, ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newTypedArray(TypedArrayType arrayType,
                                    const BytesView& bytes,
                                    ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newUntyped(const Value& value,
                                 const ReferenceInfoBuilder& referenceInfoBuilder,
                                 ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newMap(size_t capacity, ExceptionTracker& exceptionTracker) = 0;
    virtual void setMapEntry(const ValueType& map,
                             const ValueType& key,
                             const ValueType& value,
                             ExceptionTracker& exceptionTracker) = 0;
    virtual size_t getMapEstimatedLength(const ValueType& map, ExceptionTracker& exceptionTracker) = 0;
    virtual void visitMapKeyValues(const ValueType& map,
                                   PlatformMapValueVisitor<ValueType>& visitor,
                                   ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newES6Collection(CollectionType type, ExceptionTracker& exceptionTracker) = 0;
    virtual void setES6CollectionEntry(const ValueType& collection,
                                       CollectionType type,
                                       std::initializer_list<ValueType> items,
                                       ExceptionTracker& exceptionTracker) = 0;
    virtual void visitES6Collection(const ValueType& collection,
                                    PlatformMapValueVisitor<ValueType>& visitor,
                                    ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newDate(double millisecondsSinceEpoch, ExceptionTracker& exceptionTracker) = 0;

    virtual PlatformArrayBuilder<ValueType> newArrayBuilder(size_t capacity, ExceptionTracker& exceptionTracker) = 0;

    virtual void setArrayEntry(const PlatformArrayBuilder<ValueType>& arrayBuilder,
                               size_t index,
                               const ValueType& value,
                               ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType finalizeArray(PlatformArrayBuilder<ValueType>& arrayBuilder,
                                    ExceptionTracker& exceptionTracker) = 0;

    virtual PlatformArrayIterator<ValueType> newArrayIterator(const ValueType& array,
                                                              ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType getArrayItem(const PlatformArrayIterator<ValueType>& arrayIterator,
                                   size_t index,
                                   ExceptionTracker& exceptionTracker) = 0;

    virtual ValueType newBridgedPromise(const Ref<Promise>& promise,
                                        const Ref<ValueMarshaller<ValueType>>& valueMarshaller,
                                        ExceptionTracker& exceptionTracker) = 0;

    virtual Ref<PlatformObjectClassDelegate<ValueType>> newObjectClass(const Ref<ClassSchema>& schema,
                                                                       ExceptionTracker& exceptionTracker) = 0;

    virtual Ref<PlatformEnumClassDelegate<ValueType>> newEnumClass(const Ref<EnumSchema>& schema,
                                                                   ExceptionTracker& exceptionTracker) = 0;

    virtual Ref<PlatformFunctionClassDelegate<ValueType>> newFunctionClass(
        const Ref<PlatformFunctionTrampoline<ValueType>>& trampoline,
        const Ref<ValueFunctionSchema>& schema,
        ExceptionTracker& exceptionTracker) = 0;

    virtual bool valueIsNull(const ValueType& value) const = 0;
    virtual int32_t valueToInt(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;
    virtual int64_t valueToLong(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;
    virtual double valueToDouble(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;
    virtual bool valueToBool(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;

    virtual int32_t valueObjectToInt(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;
    virtual int64_t valueObjectToLong(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;
    virtual double valueObjectToDouble(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;
    virtual bool valueObjectToBool(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;

    virtual Ref<StaticString> valueToString(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;
    virtual BytesView valueToByteArray(const ValueType& value,
                                       const ReferenceInfoBuilder& referenceInfoBuilder,
                                       ExceptionTracker& exceptionTracker) const = 0;
    virtual Ref<ValueTypedArray> valueToTypedArray(const ValueType& value,
                                                   const ReferenceInfoBuilder& referenceInfoBuilder,
                                                   ExceptionTracker& exceptionTracker) const = 0;

    virtual Ref<Promise> valueToPromise(const ValueType& value,
                                        const Ref<ValueMarshaller<ValueType>>& valueMarshaller,
                                        const ReferenceInfoBuilder& referenceInfoBuilder,
                                        ExceptionTracker& exceptionTracker) const = 0;

    virtual double dateToDouble(const ValueType& value, ExceptionTracker& exceptionTracker) const = 0;

    virtual BytesView encodeProto(const ValueType& proto,
                                  const ReferenceInfoBuilder& referenceInfoBuilder,
                                  ExceptionTracker& exceptionTracker) const = 0;
    virtual ValueType decodeProto(const BytesView& bytes,
                                  const std::vector<std::string_view>& classNameParts,
                                  const ReferenceInfoBuilder& referenceInfoBuilder,
                                  ExceptionTracker& exceptionTracker) const = 0;

    virtual Value valueToUntyped(const ValueType& value,
                                 const ReferenceInfoBuilder& referenceInfoBuilder,
                                 ExceptionTracker& exceptionTracker) const = 0;

    virtual PlatformObjectStore<ValueType>& getObjectStore() = 0;
};

} // namespace Valdi
