//
//  DefaultValueMarshallers.hpp
//  valdi_core
//
//  Created by Simon Corsin on 1/25/23.
//

#pragma once

#include "valdi_core/cpp/Utils/DjinniUtils.hpp"
#include "valdi_core/cpp/Utils/InlineContainerAllocator.hpp"
#include "valdi_core/cpp/Utils/PlatformObjectAttachments.hpp"
#include "valdi_core/cpp/Utils/Promise.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueMarshaller.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include <fmt/format.h>

namespace Valdi {

template<typename ValueType>
class OptionalValueMarshaller : public ValueMarshaller<ValueType> {
public:
    OptionalValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                            const Ref<ValueMarshaller<ValueType>>& inner)
        : ValueMarshaller<ValueType>(delegate), _inner(inner) {}
    ~OptionalValueMarshaller() override = default;

    bool isOptional() const final {
        return true;
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        if (value.isNullOrUndefined()) {
            return this->_delegate->newNull();
        } else {
            return _inner->unmarshall(value, referenceInfoBuilder, exceptionTracker);
        }
    }

    Valdi::Value marshall(const ValueType* receiver,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        if (this->_delegate->valueIsNull(value)) {
            return Value::undefined();
        } else {
            return _inner->marshall(receiver, value, referenceInfoBuilder, exceptionTracker);
        }
    }

protected:
    Ref<ValueMarshaller<ValueType>> _inner;
};

template<typename ValueType>
class UndefinedValueMarshaller : public ValueMarshaller<ValueType> {
public:
    explicit UndefinedValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        return this->_delegate->newVoid();
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& /*value*/,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Value::undefined();
    }
};

template<typename ValueType>
class BoolValueMarshaller : public ValueMarshaller<ValueType> {
public:
    BoolValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate) : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<bool>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newBool(v, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->valueToBool(value, exceptionTracker));
    }
};

template<typename ValueType>
class IntValueMarshaller : public ValueMarshaller<ValueType> {
public:
    IntValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate) : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<int32_t>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newInt(v, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->valueToInt(value, exceptionTracker));
    }
};

template<typename ValueType>
class LongValueMarshaller : public ValueMarshaller<ValueType> {
public:
    LongValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate) : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<int64_t>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newLong(v, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->valueToLong(value, exceptionTracker));
    }
};

template<typename ValueType>
class DoubleValueMarshaller : public ValueMarshaller<ValueType> {
public:
    DoubleValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<double>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newDouble(v, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->valueToDouble(value, exceptionTracker));
    }
};

template<typename ValueType>
class BoolObjectValueMarshaller : public ValueMarshaller<ValueType> {
public:
    BoolObjectValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<bool>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newBoolObject(v, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->valueObjectToBool(value, exceptionTracker));
    }
};

template<typename ValueType>
class IntObjectValueMarshaller : public ValueMarshaller<ValueType> {
public:
    IntObjectValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<int32_t>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newIntObject(v, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->valueObjectToInt(value, exceptionTracker));
    }
};

template<typename ValueType>
class LongObjectValueMarshaller : public ValueMarshaller<ValueType> {
public:
    LongObjectValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<int64_t>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newLongObject(v, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->valueObjectToLong(value, exceptionTracker));
    }
};

template<typename ValueType>
class DoubleObjectValueMarshaller : public ValueMarshaller<ValueType> {
public:
    DoubleObjectValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<double>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newDoubleObject(v, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->valueObjectToDouble(value, exceptionTracker));
    }
};

template<typename ValueType>
class StringValueMarshaller : public ValueMarshaller<ValueType> {
public:
    StringValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        if (!value.isString()) {
            return this->onInvalidTypeError(exceptionTracker, Valdi::ValueType::StaticString, value);
        }

        if (value.isInternedString()) {
            return this->_delegate->newStringUTF8(value.toStringBox().toStringView(), exceptionTracker);
        } else {
            const auto* staticString = value.getStaticString();
            switch (staticString->encoding()) {
                case StaticString::Encoding::UTF8:
                    return this->_delegate->newStringUTF8(staticString->utf8StringView(), exceptionTracker);
                case StaticString::Encoding::UTF16:
                    return this->_delegate->newStringUTF16(staticString->utf16StringView(), exceptionTracker);
                case StaticString::Encoding::UTF32: {
                    auto storage = staticString->utf8Storage();
                    return this->_delegate->newStringUTF8(storage.toStringView(), exceptionTracker);
                }
            }
        }
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Value(this->_delegate->valueToString(value, exceptionTracker));
    }
};

template<typename ValueType>
class ByteArrayValueMarshaller : public ValueMarshaller<ValueType> {
public:
    ByteArrayValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto v = value.checkedTo<Ref<ValueTypedArray>>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newTypedArray(v->getType(), v->getBuffer(), exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        auto typedArray = this->_delegate->valueToTypedArray(value, referenceInfoBuilder, exceptionTracker);
        return Valdi::Value(typedArray);
    }
};

template<typename ValueType>
class UntypedValueMarshaller : public ValueMarshaller<ValueType> {
public:
    explicit UntypedValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        return this->_delegate->newUntyped(value, referenceInfoBuilder, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return this->_delegate->valueToUntyped(value, referenceInfoBuilder, exceptionTracker);
    }
};

template<typename ValueType>
class ArrayValueMarshaller : public ValueMarshaller<ValueType> {
public:
    ArrayValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                         const Ref<ValueMarshaller<ValueType>>& itemValueMarshaller)
        : ValueMarshaller<ValueType>(delegate), _itemValueMarshaller(itemValueMarshaller) {}
    ~ArrayValueMarshaller() override = default;

    ValueType handleUnmarshallArrayError(ExceptionTracker& exceptionTracker, size_t index) const {
        return this->handleUnmarshallError(exceptionTracker,
                                           fmt::format("while unmarshaling array index '{}': ", index));
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto valueArray = value.checkedTo<Ref<ValueArray>>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        auto length = valueArray->size();
        auto arrayBuilder = this->_delegate->newArrayBuilder(static_cast<size_t>(length), exceptionTracker);
        if (!exceptionTracker) {
            return this->handleUnmarshallError(exceptionTracker, "While unmarshalling array: ");
        }

        for (size_t i = 0; i < length; i++) {
            const auto& arrayItem = (*valueArray)[i];

            auto item =
                _itemValueMarshaller->unmarshall(arrayItem, referenceInfoBuilder.withArrayIndex(i), exceptionTracker);

            if (!exceptionTracker) {
                return handleUnmarshallArrayError(exceptionTracker, i);
            }

            this->_delegate->setArrayEntry(arrayBuilder, static_cast<size_t>(i), item, exceptionTracker);

            if (!exceptionTracker) {
                return handleUnmarshallArrayError(exceptionTracker, i);
            }
        }

        return this->_delegate->finalizeArray(arrayBuilder, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        auto arrayIterator = this->_delegate->newArrayIterator(value, exceptionTracker);
        if (!exceptionTracker) {
            return Valdi::Value();
        }

        size_t length = arrayIterator.size();

        auto array = ValueArray::make(length);

        for (size_t i = 0; i < length; i++) {
            auto item = this->_delegate->getArrayItem(arrayIterator, i, exceptionTracker);
            if (!exceptionTracker) {
                return this->handleMarshallError(exceptionTracker,
                                                 fmt::format("while marshalling array index '{}'", i));
            }
            auto convertedItem =
                _itemValueMarshaller->marshall(nullptr, item, referenceInfoBuilder.withArrayIndex(i), exceptionTracker);
            if (!exceptionTracker) {
                return this->handleMarshallError(exceptionTracker,
                                                 fmt::format("while marshalling array index '{}'", i));
            }

            array->emplace(i, std::move(convertedItem));
        }

        return Value(array);
    }

protected:
    Ref<ValueMarshaller<ValueType>> _itemValueMarshaller;
};

template<typename ValueType>
class MapKeyValueMarshallerVisitor : public PlatformMapValueVisitor<ValueType> {
public:
    MapKeyValueMarshallerVisitor(const Ref<ValueMap>& map,
                                 const ReferenceInfoBuilder& referenceInfoBuilder,
                                 const Ref<ValueMarshaller<ValueType>>& keyValueMarshaller,
                                 const Ref<ValueMarshaller<ValueType>>& valueValueMarshaller)
        : _map(*map),
          _referenceInfoBuilder(referenceInfoBuilder),
          _keyValueMarshaller(keyValueMarshaller),
          _valueValueMarshaller(valueValueMarshaller) {}

    bool visit(const ValueType& key, const ValueType& value, ExceptionTracker& exceptionTracker) final {
        auto resolvedKey = _keyValueMarshaller->marshall(nullptr, key, _referenceInfoBuilder, exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }

        return putMapProperty(resolvedKey.toStringBox(), value, exceptionTracker);
    }

    bool visit(const StringBox& key, const ValueType& value, ExceptionTracker& exceptionTracker) final {
        return putMapProperty(key, value, exceptionTracker);
    }

private:
    ValueMap& _map;
    const ReferenceInfoBuilder& _referenceInfoBuilder;
    const Ref<ValueMarshaller<ValueType>>& _keyValueMarshaller;
    const Ref<ValueMarshaller<ValueType>>& _valueValueMarshaller;

    bool putMapProperty(const StringBox& key, const ValueType& value, ExceptionTracker& exceptionTracker) {
        auto marshalledValue =
            _valueValueMarshaller->marshall(nullptr, value, _referenceInfoBuilder.withProperty(key), exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }

        _map[key] = marshalledValue;

        return true;
    }
};

template<typename ValueType>
class MapValueMarshaller : public ValueMarshaller<ValueType> {
public:
    MapValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                       const Ref<ValueMarshaller<ValueType>>& keyValueMarshaller,
                       const Ref<ValueMarshaller<ValueType>>& valueValueMarshaller)
        : ValueMarshaller<ValueType>(delegate),
          _keyValueMarshaller(keyValueMarshaller),
          _valueValueMarshaller(valueValueMarshaller) {}
    ~MapValueMarshaller() override = default;

    ValueType handleUnmarshallMapError(ExceptionTracker& exceptionTracker) const {
        return this->handleUnmarshallError(exceptionTracker, "While unmarshalling map: ");
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto valueMap = value.checkedTo<Ref<ValueMap>>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        auto length = valueMap->size();
        auto map = this->_delegate->newMap(static_cast<size_t>(length), exceptionTracker);
        if (!exceptionTracker) {
            return this->handleUnmarshallMapError(exceptionTracker);
        }

        for (const auto& it : *valueMap) {
            auto propertyReferenceInfoBuilder = referenceInfoBuilder.withProperty(it.first);
            auto key = _keyValueMarshaller->unmarshall(Value(it.first), propertyReferenceInfoBuilder, exceptionTracker);
            if (!exceptionTracker) {
                return this->handleUnmarshallMapError(exceptionTracker);
            }

            auto value =
                _valueValueMarshaller->unmarshall(Value(it.second), propertyReferenceInfoBuilder, exceptionTracker);
            if (!exceptionTracker) {
                return this->handleUnmarshallMapError(exceptionTracker);
            }

            this->_delegate->setMapEntry(map, key, value, exceptionTracker);

            if (!exceptionTracker) {
                return this->handleUnmarshallMapError(exceptionTracker);
            }
        }

        return map;
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        auto length = this->_delegate->getMapEstimatedLength(value, exceptionTracker);
        if (!exceptionTracker) {
            return this->handleMarshallError(exceptionTracker, "While marshalling map: ");
        }

        auto map = makeShared<ValueMap>();
        map->reserve(length);

        MapKeyValueMarshallerVisitor<ValueType> visitor(
            map, referenceInfoBuilder, _keyValueMarshaller, _valueValueMarshaller);

        this->_delegate->visitMapKeyValues(value, visitor, exceptionTracker);

        if (!exceptionTracker) {
            return this->handleMarshallError(exceptionTracker, "While marshalling map: ");
        }

        return Valdi::Value(map);
    }

protected:
    Ref<ValueMarshaller<ValueType>> _keyValueMarshaller;
    Ref<ValueMarshaller<ValueType>> _valueValueMarshaller;
};

template<typename ValueType>
class ES6MapKeyValueMarshallerVisitor : public PlatformMapValueVisitor<ValueType> {
public:
    ES6MapKeyValueMarshallerVisitor(const Ref<ES6Map>& es6map,
                                    const ReferenceInfoBuilder& referenceInfoBuilder,
                                    const Ref<ValueMarshaller<ValueType>>& keyValueMarshaller,
                                    const Ref<ValueMarshaller<ValueType>>& valueValueMarshaller)
        : _es6map(*es6map),
          _referenceInfoBuilder(referenceInfoBuilder),
          _keyValueMarshaller(keyValueMarshaller),
          _valueValueMarshaller(valueValueMarshaller) {}

    bool visit(const ValueType& key, const ValueType& value, ExceptionTracker& exceptionTracker) final {
        auto marshalledKey = _keyValueMarshaller->marshall(nullptr, key, _referenceInfoBuilder, exceptionTracker);
        auto marshalledValue = _valueValueMarshaller->marshall(
            nullptr, value, _referenceInfoBuilder.withProperty(marshalledKey.toStringBox()), exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }

        _es6map.entries.push_back(std::move(marshalledKey));
        _es6map.entries.push_back(std::move(marshalledValue));

        return true;
    }
    bool visit(const StringBox& key, const ValueType& value, ExceptionTracker& exceptionTracker) final {
        auto marshalledValue =
            _valueValueMarshaller->marshall(nullptr, value, _referenceInfoBuilder.withProperty(key), exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }

        _es6map.entries.push_back(Value(key));
        _es6map.entries.push_back(std::move(marshalledValue));

        return true;
    }

private:
    ES6Map& _es6map;
    const ReferenceInfoBuilder& _referenceInfoBuilder;
    const Ref<ValueMarshaller<ValueType>>& _keyValueMarshaller;
    const Ref<ValueMarshaller<ValueType>>& _valueValueMarshaller;
};

template<typename ValueType>
class ES6MapValueMarshaller : public ValueMarshaller<ValueType> {
public:
    ES6MapValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                          const Ref<ValueMarshaller<ValueType>>& keyValueMarshaller,
                          const Ref<ValueMarshaller<ValueType>>& valueValueMarshaller)
        : ValueMarshaller<ValueType>(delegate),
          _keyValueMarshaller(keyValueMarshaller),
          _valueValueMarshaller(valueValueMarshaller) {}
    ~ES6MapValueMarshaller() override = default;

    ValueType handleUnmarshallMapError(ExceptionTracker& exceptionTracker) const {
        return this->handleUnmarshallError(exceptionTracker, "While unmarshalling es6map: ");
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto es6map = value.checkedToValdiObject<ES6Map>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        auto map = this->_delegate->newES6Collection(CollectionType::Map, exceptionTracker);
        if (!exceptionTracker) {
            return this->handleUnmarshallMapError(exceptionTracker);
        }
        const auto& entries = es6map->entries;
        for (size_t i = 0; i < entries.size(); i += 2) {
            auto propertyReferenceInfoBuilder = referenceInfoBuilder.withProperty(entries[i].toStringBox());
            auto key = _keyValueMarshaller->unmarshall(entries[i], propertyReferenceInfoBuilder, exceptionTracker);
            if (!exceptionTracker) {
                return this->handleUnmarshallMapError(exceptionTracker);
            }

            auto value =
                _valueValueMarshaller->unmarshall(entries[i + 1], propertyReferenceInfoBuilder, exceptionTracker);
            if (!exceptionTracker) {
                return this->handleUnmarshallMapError(exceptionTracker);
            }

            this->_delegate->setES6CollectionEntry(
                map, CollectionType::Map, {std::move(key), std::move(value)}, exceptionTracker);

            if (!exceptionTracker) {
                return this->handleUnmarshallMapError(exceptionTracker);
            }
        }

        return map;
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        auto es6map = makeShared<ES6Map>();

        ES6MapKeyValueMarshallerVisitor<ValueType> visitor(
            es6map, referenceInfoBuilder, _keyValueMarshaller, _valueValueMarshaller);

        this->_delegate->visitES6Collection(value, visitor, exceptionTracker);

        if (!exceptionTracker) {
            return this->handleMarshallError(exceptionTracker, "While marshalling es6map: ");
        }

        return Valdi::Value(es6map);
    }

protected:
    Ref<ValueMarshaller<ValueType>> _keyValueMarshaller;
    Ref<ValueMarshaller<ValueType>> _valueValueMarshaller;
};

template<typename ValueType>
class ES6SetMarshallerVisitor : public PlatformMapValueVisitor<ValueType> {
public:
    ES6SetMarshallerVisitor(const Ref<ES6Set>& es6set,
                            const ReferenceInfoBuilder& referenceInfoBuilder,
                            const Ref<ValueMarshaller<ValueType>>& keyValueMarshaller)
        : _es6set(*es6set), _referenceInfoBuilder(referenceInfoBuilder), _keyValueMarshaller(keyValueMarshaller) {}

    bool visit(const ValueType& key, const ValueType& value, ExceptionTracker& exceptionTracker) final {
        auto marshalledKey = _keyValueMarshaller->marshall(nullptr, key, _referenceInfoBuilder, exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }
        _es6set.entries.push_back(std::move(marshalledKey));
        return true;
    }
    bool visit(const StringBox& key, const ValueType& value, ExceptionTracker& exceptionTracker) final {
        _es6set.entries.push_back(Value(key));
        return true;
    }

private:
    ES6Set& _es6set;
    const ReferenceInfoBuilder& _referenceInfoBuilder;
    const Ref<ValueMarshaller<ValueType>>& _keyValueMarshaller;
};

template<typename ValueType>
class ES6SetValueMarshaller : public ValueMarshaller<ValueType> {
public:
    ES6SetValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                          const Ref<ValueMarshaller<ValueType>>& keyValueMarshaller)
        : ValueMarshaller<ValueType>(delegate), _keyValueMarshaller(keyValueMarshaller) {}
    ~ES6SetValueMarshaller() override = default;

    ValueType handleUnmarshallSetError(ExceptionTracker& exceptionTracker) const {
        return this->handleUnmarshallError(exceptionTracker, "While unmarshalling es6set: ");
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto es6set = value.checkedToValdiObject<ES6Set>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }
        auto set = this->_delegate->newES6Collection(CollectionType::Set, exceptionTracker);
        if (!exceptionTracker) {
            return this->handleUnmarshallSetError(exceptionTracker);
        }
        const auto& entries = es6set->entries;
        for (size_t i = 0; i < entries.size(); i++) {
            auto propertyReferenceInfoBuilder = referenceInfoBuilder.withProperty(entries[i].toStringBox());
            auto key = _keyValueMarshaller->unmarshall(entries[i], propertyReferenceInfoBuilder, exceptionTracker);
            if (!exceptionTracker) {
                return this->handleUnmarshallSetError(exceptionTracker);
            }
            this->_delegate->setES6CollectionEntry(set, CollectionType::Set, {std::move(key)}, exceptionTracker);
            if (!exceptionTracker) {
                return this->handleUnmarshallSetError(exceptionTracker);
            }
        }
        return set;
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        auto es6set = makeShared<ES6Set>();
        ES6SetMarshallerVisitor<ValueType> visitor(es6set, referenceInfoBuilder, _keyValueMarshaller);
        this->_delegate->visitES6Collection(value, visitor, exceptionTracker);
        if (!exceptionTracker) {
            return this->handleMarshallError(exceptionTracker, "While marshalling es6set: ");
        }
        return Valdi::Value(es6set);
    }

protected:
    Ref<ValueMarshaller<ValueType>> _keyValueMarshaller;
};

template<typename ValueType>
class OutcomeVisitor : public PlatformMapValueVisitor<ValueType> {
public:
    OutcomeVisitor(ValdiOutcome& outcome,
                   const ReferenceInfoBuilder& referenceInfoBuilder,
                   const Ref<StringValueMarshaller<ValueType>>& stringValueMarshaller,
                   const Ref<ValueMarshaller<ValueType>>& valueValueMarshaller,
                   const Ref<ValueMarshaller<ValueType>>& errorValueMarshaller)
        : _outcome(outcome),
          _referenceInfoBuilder(referenceInfoBuilder),
          _stringValueMarshaller(stringValueMarshaller),
          _valueValueMarshaller(valueValueMarshaller),
          _errorValueMarshaller(errorValueMarshaller) {}

    bool visit(const ValueType& key, const ValueType& value, ExceptionTracker& exceptionTracker) final {
        auto keyValue = _stringValueMarshaller->marshall(nullptr, key, _referenceInfoBuilder, exceptionTracker);
        auto keyString = keyValue.toStringBox();
        return visit(keyString, value, exceptionTracker);
    }

    bool visit(const StringBox& key, const ValueType& value, ExceptionTracker& exceptionTracker) final {
        if (key.toStringView() == "result") {
            _outcome.result = _valueValueMarshaller->marshall(nullptr, value, _referenceInfoBuilder, exceptionTracker);
            if (exceptionTracker) {
                return true;
            }
        } else if (key.toStringView() == "error") {
            _outcome.error = _errorValueMarshaller->marshall(nullptr, value, _referenceInfoBuilder, exceptionTracker);
            if (exceptionTracker) {
                return true;
            }
        }
        return false;
    }

private:
    ValdiOutcome& _outcome;
    const ReferenceInfoBuilder& _referenceInfoBuilder;
    const Ref<StringValueMarshaller<ValueType>>& _stringValueMarshaller;
    const Ref<ValueMarshaller<ValueType>>& _valueValueMarshaller;
    const Ref<ValueMarshaller<ValueType>>& _errorValueMarshaller;
};

template<typename ValueType>
class OutcomeValueMarshaller : public ValueMarshaller<ValueType> {
public:
    OutcomeValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                           const Ref<ValueMarshaller<ValueType>>& valueValueMarshaller,
                           const Ref<ValueMarshaller<ValueType>>& errorValueMarshaller)
        : ValueMarshaller<ValueType>(delegate),
          _stringValueMarshaller(makeShared<StringValueMarshaller<ValueType>>(delegate)),
          _valueValueMarshaller(valueValueMarshaller),
          _errorValueMarshaller(errorValueMarshaller) {}
    ~OutcomeValueMarshaller() override = default;

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto outcomeCpp = value.checkedToValdiObject<ValdiOutcome>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }
        auto outcomeJs = this->_delegate->newMap(1, exceptionTracker);
        if (!exceptionTracker) {
            return this->handleUnmarshallError(exceptionTracker, "While unmarshalling outcome: ");
        }
        if (outcomeCpp->error.isUndefined()) {
            auto key = this->_delegate->newStringUTF8("result", exceptionTracker);
            auto value = _valueValueMarshaller->unmarshall(outcomeCpp->result, referenceInfoBuilder, exceptionTracker);
            this->_delegate->setMapEntry(outcomeJs, key, value, exceptionTracker);
        } else {
            auto key = this->_delegate->newStringUTF8("error", exceptionTracker);
            auto error = _errorValueMarshaller->unmarshall(outcomeCpp->error, referenceInfoBuilder, exceptionTracker);
            this->_delegate->setMapEntry(outcomeJs, key, error, exceptionTracker);
        }
        return outcomeJs;
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        auto outcomeCpp = makeShared<ValdiOutcome>();
        OutcomeVisitor<ValueType> visitor(
            *outcomeCpp, referenceInfoBuilder, _stringValueMarshaller, _valueValueMarshaller, _errorValueMarshaller);
        this->_delegate->visitMapKeyValues(value, visitor, exceptionTracker);
        if (!exceptionTracker) {
            return this->handleMarshallError(exceptionTracker, "While marshalling outcome: ");
        }
        return Valdi::Value(outcomeCpp);
    }

protected:
    Ref<StringValueMarshaller<ValueType>> _stringValueMarshaller;
    Ref<ValueMarshaller<ValueType>> _valueValueMarshaller;
    Ref<ValueMarshaller<ValueType>> _errorValueMarshaller;
};

template<typename ValueType>
class DateValueMarshaller : public ValueMarshaller<ValueType> {
public:
    DateValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate) : ValueMarshaller<ValueType>(delegate) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto millisecondsSinceEpoch = value.checkedTo<double>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }
        return this->_delegate->newDate(millisecondsSinceEpoch, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(this->_delegate->dateToDouble(value, exceptionTracker));
    }
};

template<typename ValueType>
class ProtoValueMarshaller : public ValueMarshaller<ValueType> {
public:
    ProtoValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                         const std::vector<std::string_view>& classNameParts)
        : ValueMarshaller<ValueType>(delegate), _classNameParts(classNameParts) {}

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        // Byte array in Valdi::Value to JS proto
        const auto& buffer = value.getTypedArrayRef()->getBuffer();
        return this->_delegate->decodeProto(buffer, _classNameParts, referenceInfoBuilder, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        // JS proto to byte array in Valdi::Value
        auto buffer = this->_delegate->encodeProto(value, referenceInfoBuilder, exceptionTracker);
        auto array = makeShared<ValueTypedArray>(kDefaultTypedArrayType, buffer);
        return Valdi::Value(array);
    }

private:
    const std::vector<std::string_view> _classNameParts;
};

template<typename ValueType>
class PromiseValueMarshaller : public ValueMarshaller<ValueType> {
public:
    PromiseValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                           const Ref<ValueMarshaller<ValueType>>& valueMarshaller)
        : ValueMarshaller<ValueType>(delegate), _valueMarshaller(valueMarshaller) {}
    ~PromiseValueMarshaller() override = default;

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto promise = value.checkedToValdiObject<Promise>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return this->_delegate->newBridgedPromise(promise, _valueMarshaller, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return Valdi::Value(
            this->_delegate->valueToPromise(value, _valueMarshaller, referenceInfoBuilder, exceptionTracker));
    }

protected:
    Ref<ValueMarshaller<ValueType>> _valueMarshaller;
};

template<typename ValueType>
class FunctionValueMarshaller : public ValueMarshaller<ValueType> {
public:
    FunctionValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                            Ref<PlatformFunctionClassDelegate<ValueType>> functionClass)
        : ValueMarshaller<ValueType>(delegate), _functionClass(std::move(functionClass)) {}
    ~FunctionValueMarshaller() override = default;

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        auto function = value.checkedTo<Ref<ValueFunction>>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        return _functionClass->newFunction(function, referenceInfoBuilder.asFunction(), exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* receiver,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        auto valueFunction =
            _functionClass->toValueFunction(receiver, value, referenceInfoBuilder.asFunction(), exceptionTracker);
        return Value(valueFunction);
    }

protected:
    Ref<PlatformFunctionClassDelegate<ValueType>> _functionClass;
};

template<typename ValueType>
class ObjectValueMarshaller : public ValueMarshaller<ValueType> {
public:
    ObjectValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                          const Ref<ClassSchema>& schema,
                          Ref<PlatformObjectClassDelegate<ValueType>> objectClass)
        : ValueMarshaller<ValueType>(delegate), _schema(schema), _objectClass(std::move(objectClass)) {
        InlineContainerAllocator<ObjectValueMarshaller<ValueType>, Ref<ValueMarshaller<ValueType>>> allocator;
        allocator.constructContainerEntries(this, schema->getPropertiesSize());
    }

    ~ObjectValueMarshaller() override {
        InlineContainerAllocator<ObjectValueMarshaller<ValueType>, Ref<ValueMarshaller<ValueType>>> allocator;
        allocator.deallocate(this, _schema->getPropertiesSize());
    }

    const Ref<ValueMarshaller<ValueType>>& getPropertyMarshaller(size_t index) const {
        InlineContainerAllocator<ObjectValueMarshaller<ValueType>, Ref<ValueMarshaller<ValueType>>> allocator;
        return allocator.getContainerStartPtr(this)[index];
    }

    void setPropertyMarshaller(size_t index, Ref<ValueMarshaller<ValueType>> propertyMarshaller) {
        InlineContainerAllocator<ObjectValueMarshaller<ValueType>, Ref<ValueMarshaller<ValueType>>> allocator;
        allocator.getContainerStartPtr(this)[index] = std::move(propertyMarshaller);
    }

    static std::string getUnmarshallPropertyErrorMessage(ClassSchema& schema, const ClassPropertySchema& property) {
        return fmt::format("Failed to unmarshall property '{}' of class '{}'", property.name, schema.getClassName());
    }

    static std::string getMarshallPropertyErrorMessage(ClassSchema& schema, const ClassPropertySchema& property) {
        return fmt::format("While marshalling property '{}' of class '{}': ", property.name, schema.getClassName());
    }

    ValueType handleUnmarshallPropertyError(const ClassPropertySchema& property, ExceptionTracker& exceptionTracker) {
        return this->handleUnmarshallError(exceptionTracker, getUnmarshallPropertyErrorMessage(*_schema, property));
    }

    ValueType doUnmarshall(const Valdi::Value& value,
                           const ReferenceInfoBuilder& referenceInfoBuilder,
                           ExceptionTracker& exceptionTracker) {
        const auto* typedObject = value.getTypedObject();
        const auto* map = value.getMap();

        if (typedObject == nullptr && map == nullptr) {
            return this->onInvalidTypeError(exceptionTracker, Valdi::ValueType::TypedObject, value);
        }

        auto propertiesSize = _schema->getPropertiesSize();

        ValueType properties[propertiesSize];

        for (size_t i = 0; i < propertiesSize; i++) {
            const auto& property = _schema->getProperty(i);
            const auto& propertyMarshaller = getPropertyMarshaller(i);

            auto propertyValue = Value::undefined();

            if (typedObject != nullptr) {
                propertyValue = typedObject->getProperty(i);
            } else {
                const auto& it = map->find(property.name);
                if (it != map->end()) {
                    propertyValue = it->second;
                }
            }

            properties[i] = propertyMarshaller->unmarshall(
                propertyValue, referenceInfoBuilder.withProperty(property.name), exceptionTracker);

            if (!exceptionTracker) {
                return handleUnmarshallPropertyError(property, exceptionTracker);
            }
        }

        return _objectClass->newObject(&properties[0], exceptionTracker);
    }

    ValueType unmarshallInterface(const Valdi::Value& value,
                                  const ReferenceInfoBuilder& referenceInfoBuilder,
                                  ExceptionTracker& exceptionTracker) {
        auto& objectStore = this->_delegate->getObjectStore();
        auto proxyObject = value.checkedTo<Ref<ValueTypedProxyObject>>(exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        std::lock_guard<std::recursive_mutex> lock(objectStore.mutex());

        // Attempt to resolve an existing object for the given proxy
        auto object = objectStore.getObjectForId(proxyObject->getId(), exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        if (object) {
            return std::move(object.value());
        }

        // No existing object for this proxy, create one

        auto result = doUnmarshall(Value(proxyObject->getTypedObject()), referenceInfoBuilder, exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        /**
         The following line allows the created object to be retrievable
         from the proxy id. Next time the same proxy needs to be unmarshalled,
         the unmarshalled object representation will be unwrapped back.
         The object store is responsible for holding a weak reference on that
         object, to allow the unmarshalled object to be garbage collected
         once it is not in use anymore.
         */
        objectStore.setObjectForId(proxyObject->getId(), result, exceptionTracker);
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        /**
         The following lines allow the source proxy to be retrievable from
         the created object. If the created object goes through a marshall
         pass, the source proxy will be unwrapped. The created object will
         hold a strong reference on the proxy, making the proxy available
         for as long as the created object is alive.
         */

        auto attachments =
            castOrNull<PlatformObjectAttachments>(objectStore.getValueForObjectKey(result, exceptionTracker));
        if (!exceptionTracker) {
            return this->_delegate->newNull();
        }

        if (attachments == nullptr) {
            attachments = makeShared<PlatformObjectAttachments>();
            objectStore.setValueForObjectKey(result, attachments, exceptionTracker);
            if (!exceptionTracker) {
                return this->_delegate->newNull();
            }
        }

        attachments->setProxyForSource(this, proxyObject, true);

        return result;
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        if (_schema->isInterface()) {
            return unmarshallInterface(value, referenceInfoBuilder, exceptionTracker);
        } else {
            return doUnmarshall(value, referenceInfoBuilder, exceptionTracker);
        }
    }

    Valdi::Value marshallTypedObject(const ValueType* receiver,
                                     const ValueType& value,
                                     const ReferenceInfoBuilder& referenceInfoBuilder,
                                     ExceptionTracker& exceptionTracker) {
        auto propertiesSize = _schema->getPropertiesSize();
        auto typedObject = ValueTypedObject::make(_schema);

        for (size_t i = 0; i < propertiesSize; i++) {
            const auto& property = _schema->getProperty(i);
            const auto& propertyMarshaller = getPropertyMarshaller(i);

            auto propertyValue = _objectClass->getProperty(value, i, exceptionTracker);
            if (!exceptionTracker) {
                return this->handleMarshallError(exceptionTracker, getMarshallPropertyErrorMessage(*_schema, property));
            }

            auto marshalledPropertyValue = propertyMarshaller->marshall(
                receiver, propertyValue, referenceInfoBuilder.withProperty(property.name), exceptionTracker);
            if (!exceptionTracker) {
                return this->handleMarshallError(exceptionTracker, getMarshallPropertyErrorMessage(*_schema, property));
            }

            typedObject->setProperty(i, marshalledPropertyValue);
        }

        return Value(typedObject);
    }

    Valdi::Value marshallInterface(const ValueType& value,
                                   const ReferenceInfoBuilder& referenceInfoBuilder,
                                   ExceptionTracker& exceptionTracker) {
        auto& objectStore = this->_delegate->getObjectStore();
        std::lock_guard<std::recursive_mutex> lock(objectStore.mutex());

        auto attachments =
            castOrNull<PlatformObjectAttachments>(objectStore.getValueForObjectKey(value, exceptionTracker));
        if (!exceptionTracker) {
            return Value();
        }

        Ref<ValueTypedProxyObject> proxyObject;
        if (attachments != nullptr) {
            proxyObject = attachments->getProxyForSource(this);
            if (proxyObject != nullptr) {
                return Value(proxyObject);
            }
        } else {
            attachments = makeShared<PlatformObjectAttachments>();
            objectStore.setValueForObjectKey(value, attachments, exceptionTracker);
            if (!exceptionTracker) {
                return Value();
            }
        }

        // No existing proxy, we create one

        auto typedObjectIndex = marshallTypedObject(&value, value, referenceInfoBuilder, exceptionTracker);
        if (!exceptionTracker) {
            return Value();
        }

        proxyObject = _objectClass->newProxy(value, typedObjectIndex.getTypedObjectRef(), exceptionTracker);
        if (!exceptionTracker) {
            return this->handleMarshallError(
                exceptionTracker, fmt::format("While creating proxy object for class: {}: ", _schema->getClassName()));
        }

        /**
         The following line allows the newly created proxy to be re-used next time the protocol
         is marshalled again. The proxy is held as a weak reference on the object, since the object
         should not take a strong reference on its own proxy to prevent retain cycles. The target
         platform that will received the proxy is responsible for retaining it.
         */
        attachments->setProxyForSource(this, proxyObject, false);

        /**
         The following line allows the object to be retrieved from the proxy, so that if the proxy
         is passed back to be unmarshalled, we can retrieve the source object.
         */
        objectStore.setObjectForId(proxyObject->getId(), value, exceptionTracker);
        if (!exceptionTracker) {
            return Value();
        }

        return Value(proxyObject);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        if (_schema->isInterface()) {
            return marshallInterface(value, referenceInfoBuilder, exceptionTracker);
        } else {
            return marshallTypedObject(nullptr, value, referenceInfoBuilder, exceptionTracker);
        }
    }

    static Ref<ObjectValueMarshaller<ValueType>> make(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                                                      const Ref<ClassSchema>& schema,
                                                      Ref<PlatformObjectClassDelegate<ValueType>> objectClass) {
        InlineContainerAllocator<ObjectValueMarshaller<ValueType>, Ref<ValueMarshaller<ValueType>>> allocator;
        return allocator.allocate(schema->getPropertiesSize(), delegate, schema, std::move(objectClass));
    }

protected:
    Ref<ClassSchema> _schema;
    Ref<PlatformObjectClassDelegate<ValueType>> _objectClass;

    friend InlineContainerAllocator<ObjectValueMarshaller<ValueType>, Ref<ValueMarshaller<ValueType>>>;
};

template<typename ValueType>
class EnumValueMarshaller : public ValueMarshaller<ValueType> {
public:
    EnumValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate,
                        const Ref<EnumSchema>& schema,
                        Ref<PlatformEnumClassDelegate<ValueType>> enumClass,
                        bool isBoxed)
        : ValueMarshaller<ValueType>(delegate), _schema(schema), _enumClass(std::move(enumClass)), _isBoxed(isBoxed) {}

    ~EnumValueMarshaller() override = default;

    ValueType handleUnmarshallEnumError(ExceptionTracker& exceptionTracker) {
        return this->handleUnmarshallError(exceptionTracker,
                                           fmt::format("While unmarshalling enum '{}': ", _schema->getName()));
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        for (size_t i = 0; i < _schema->getCasesSize(); i++) {
            const auto& enumCase = _schema->getCase(i);

            if (enumCase.value == value) {
                return _enumClass->newEnum(i, _isBoxed, exceptionTracker);
            }
        }

        exceptionTracker.onError(fmt::format("Invalid enum case '{}'", value.toString()));

        return handleUnmarshallEnumError(exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* /*receiver*/,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return _enumClass->enumCaseToValue(value, _isBoxed, exceptionTracker);
    }

protected:
    Ref<EnumSchema> _schema;
    Ref<PlatformEnumClassDelegate<ValueType>> _enumClass;
    bool _isBoxed;
};

template<typename ValueType>
class IndirectValueMarshaller : public ValueMarshaller<ValueType> {
public:
    IndirectValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate, bool optional)
        : ValueMarshaller<ValueType>(delegate), _optional(optional) {}
    ~IndirectValueMarshaller() override = default;

    bool isOptional() const final {
        return _optional;
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        if (_inner == nullptr) {
            return this->handleUnmarshallError(exceptionTracker, "Unresolved indirect ValueMarshaller");
        }
        if (_optional && value.isNullOrUndefined()) {
            return this->_delegate->newNull();
        }

        auto output = _inner->unmarshall(value, referenceInfoBuilder, exceptionTracker);

        if (_processor != nullptr && exceptionTracker) {
            return _processor->postprocess(output, referenceInfoBuilder, exceptionTracker);
        } else {
            return output;
        }
    }

    Valdi::Value marshall(const ValueType* receiver,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        if (_inner == nullptr) {
            return this->handleMarshallError(exceptionTracker, "Unresolved indirect ValueMarshaller");
        }
        if (_optional && this->_delegate->valueIsNull(value)) {
            return Value::undefined();
        }

        if (_processor != nullptr) {
            auto preprocessedValue = _processor->preprocess(receiver, value, referenceInfoBuilder, exceptionTracker);
            if (!exceptionTracker) {
                return Valdi::Value();
            }
            return _inner->marshall(nullptr, preprocessedValue, referenceInfoBuilder, exceptionTracker);
        } else {
            return _inner->marshall(receiver, value, referenceInfoBuilder, exceptionTracker);
        }
    }

    ValueMarshaller<ValueType>* getInner() const {
        return _inner;
    }

    void setInner(ValueMarshaller<ValueType>* inner) {
        _inner = inner;
    }

    void setProcessor(const Ref<ValueMarshallerProcessor<ValueType>>& processor) {
        _processor = processor;
    }

protected:
    ValueMarshaller<ValueType>* _inner = nullptr;
    Ref<ValueMarshallerProcessor<ValueType>> _processor;
    bool _optional;
};

/**
 ValueMarshaller implementation that uses a different instance for marshalling vs unmarshalling
 */
template<typename ValueType>
class UnbalancedValueMarshaller : public ValueMarshaller<ValueType> {
public:
    explicit UnbalancedValueMarshaller(const Ref<PlatformValueDelegate<ValueType>>& delegate)
        : ValueMarshaller<ValueType>(delegate) {}
    ~UnbalancedValueMarshaller() override = default;

    bool isOptional() const final {
        return (_marshaller != nullptr && _marshaller->isOptional()) ||
               (_unmarshaller != nullptr && _unmarshaller->isOptional());
    }

    ValueType unmarshall(const Valdi::Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) final {
        return _unmarshaller->unmarshall(value, referenceInfoBuilder, exceptionTracker);
    }

    Valdi::Value marshall(const ValueType* receiver,
                          const ValueType& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final {
        return _marshaller->marshall(receiver, value, referenceInfoBuilder, exceptionTracker);
    }

    void setMarshaller(const Ref<ValueMarshaller<ValueType>>& marshaller) {
        _marshaller = marshaller;
    }

    void setUnmarshaller(const Ref<ValueMarshaller<ValueType>>& unmarshaller) {
        _unmarshaller = unmarshaller;
    }

private:
    Ref<ValueMarshaller<ValueType>> _marshaller;
    Ref<ValueMarshaller<ValueType>> _unmarshaller;
};

} // namespace Valdi
