//
//  ProtobufModule.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/21/19.
//

#include "valdi/runtime/JavaScript/Modules/ProtobufModule.hpp"
#include "valdi/runtime/JavaScript/JSFunctionWithMethod.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/JavaScript/Modules/ProtobufArena.hpp"
#include "valdi/runtime/JavaScript/Modules/ProtobufMessageFactory.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"

#include "valdi_core/cpp/JavaScript/JavaScriptPathResolver.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

#include "valdi_protobuf/Message.hpp"

#include "utils/debugging/Assert.hpp"

namespace Valdi {

class ProtobufArenaAccess {
public:
    explicit ProtobufArenaAccess(Ref<ProtobufArena>&& arena)
        : _arena(std::move(arena)),
          _lock(_arena != nullptr ? _arena->lock() : std::unique_lock<std::recursive_mutex>()) {}
    ~ProtobufArenaAccess() = default;

    constexpr ProtobufArena* operator->() const {
        return _arena.get();
    }

    constexpr ProtobufArena& operator*() const {
        return *_arena;
    }

private:
    Ref<ProtobufArena> _arena;
    std::unique_lock<std::recursive_mutex> _lock;
};

template<typename T>
static Ref<T> getNativeObject(JSFunctionNativeCallContext& callContext, size_t index, const char* name) {
    auto result = jsWrappedObjectToValdiObject(
        callContext.getContext(), callContext.getParameter(index), callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return Ref<T>();
    }

    auto nativeObject = castOrNull<T>(result);
    if (nativeObject == nullptr) {
        if (result != nullptr) {
            callContext.throwError(Error(STRING_FORMAT("Cannot cast {} to {}", result->getClassName(), name)));
        } else {
            callContext.throwError(Error(STRING_FORMAT("Missing attached native {}", name)));
        }
    }

    return nativeObject;
}

static Ref<ProtobufArena> getArenaUnsafe(JSFunctionNativeCallContext& callContext, size_t index) {
    return getNativeObject<ProtobufArena>(callContext, index, "arena");
}

static ProtobufArenaAccess getArena(JSFunctionNativeCallContext& callContext, size_t index) {
    return ProtobufArenaAccess(getArenaUnsafe(callContext, index));
}

static Ref<ProtobufMessageFactory> getMessageFactory(JSFunctionNativeCallContext& callContext, size_t index) {
    return getNativeObject<ProtobufMessageFactory>(callContext, index, "message factory");
}

static size_t jsValueToIndex(JSFunctionNativeCallContext& callContext, const JSValue& value) {
    return static_cast<size_t>(callContext.getContext().valueToInt(value, callContext.getExceptionTracker()));
}

static size_t getIndex(JSFunctionNativeCallContext& callContext, size_t index) {
    auto parameter = callContext.getParameter(index);
    return jsValueToIndex(callContext, parameter);
}

static JSValueRef toJSIndex(JSFunctionNativeCallContext& callContext, size_t index) {
    return callContext.getContext().newNumber(static_cast<int32_t>(index));
}

static void notifyAsyncCallbackError(const Ref<ValueFunction>& callback, const Error& error) {
    (*callback)({
        Value::undefined(),
        Value(error.toStringBox()),
    });
}

// FIELD GETTERS

static JSValueRef getProtobufNonRepeatedField(ProtobufArena& arena,
                                              JSProtobufMessage& message,
                                              Protobuf::Field& field,
                                              const google::protobuf::FieldDescriptor* fieldDescriptor,
                                              JSFunctionNativeCallContext& callContext) {
    auto& jsContext = callContext.getContext();
    switch (fieldDescriptor->type()) {
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
            return jsContext.newNumber(field.getDouble());
        case google::protobuf::FieldDescriptor::TYPE_FLOAT:
            return jsContext.newNumber(static_cast<double>(field.getFloat()));
        case google::protobuf::FieldDescriptor::TYPE_INT64:
            return jsContext.newLong(field.getInt64(), callContext.getExceptionTracker());
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
            return jsContext.newUnsignedLong(field.getUInt64(), callContext.getExceptionTracker());
        case google::protobuf::FieldDescriptor::TYPE_INT32:
            return jsContext.newNumber(field.getInt32());
        case google::protobuf::FieldDescriptor::TYPE_FIXED64:
            return jsContext.newUnsignedLong(field.getFixed64(), callContext.getExceptionTracker());
        case google::protobuf::FieldDescriptor::TYPE_FIXED32:
            return jsContext.newNumber(static_cast<double>(field.getFixed32()));
        case google::protobuf::FieldDescriptor::TYPE_BOOL:
            return jsContext.newBool(field.getBool());
        case google::protobuf::FieldDescriptor::TYPE_STRING: {
            auto raw = field.getRaw();
            if (raw.data != nullptr) {
                return jsContext.newStringUTF8(std::string_view(reinterpret_cast<const char*>(raw.data), raw.length),
                                               callContext.getExceptionTracker());
            }

            const auto* string = field.getString();
            if (string != nullptr) {
                switch (string->encoding()) {
                    case StaticString::Encoding::UTF8:
                        return jsContext.newStringUTF8(string->utf8StringView(), callContext.getExceptionTracker());
                    case StaticString::Encoding::UTF16:
                        return jsContext.newStringUTF16(string->utf16StringView(), callContext.getExceptionTracker());
                    case StaticString::Encoding::UTF32: {
                        auto storage = string->utf8Storage();
                        return jsContext.newStringUTF8(storage.toStringView(), callContext.getExceptionTracker());
                    }
                }
            }

            return jsContext.newUndefined();
        }
        case google::protobuf::FieldDescriptor::TYPE_GROUP:
            return jsContext.newUndefined();
        case google::protobuf::FieldDescriptor::TYPE_MESSAGE: {
            auto messageIndex = arena.fieldToMessageIndex(
                message, field, fieldDescriptor->message_type(), callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);

            return toJSIndex(callContext, messageIndex);
        }
        case google::protobuf::FieldDescriptor::TYPE_BYTES: {
            auto raw = field.getRaw();
            if (raw.data != nullptr) {
                return newTypedArrayFromBytesView(jsContext,
                                                  TypedArrayType::Uint8Array,
                                                  Valdi::BytesView(message.getDataSource(), raw.data, raw.length),
                                                  callContext.getExceptionTracker());
            }

            auto* typedArray = field.getTypedArray();

            if (typedArray != nullptr) {
                return newTypedArrayFromBytesView(
                    jsContext, TypedArrayType::Uint8Array, typedArray->getBuffer(), callContext.getExceptionTracker());
            }

            return jsContext.newUndefined();
        }
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
            return jsContext.newNumber(static_cast<double>(field.getUInt32()));
        case google::protobuf::FieldDescriptor::TYPE_ENUM:
            return jsContext.newNumber(static_cast<double>(field.getEnum()));
        case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
            return jsContext.newNumber(static_cast<double>(field.getSFixed32()));
        case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
            return jsContext.newLong(field.getSFixed64(), callContext.getExceptionTracker());
        case google::protobuf::FieldDescriptor::TYPE_SINT32:
            return jsContext.newNumber(static_cast<double>(field.getSInt32()));
        case google::protobuf::FieldDescriptor::TYPE_SINT64:
            return jsContext.newLong(field.getSInt64(), callContext.getExceptionTracker());
    }
}

static JSValueRef getProtobufRepeatedField(ProtobufArena& arena,
                                           JSProtobufMessage& message,
                                           Protobuf::Field& field,
                                           const google::protobuf::FieldDescriptor* fieldDescriptor,
                                           JSFunctionNativeCallContext& callContext) {
    auto* repeated = field.toRepeated(*fieldDescriptor);

    auto array = callContext.getContext().newArray(repeated->size(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    size_t i = 0;
    for (auto& value : *repeated) {
        auto conversionResult = getProtobufNonRepeatedField(arena, message, value, fieldDescriptor, callContext);
        CHECK_CALL_CONTEXT(callContext);
        callContext.getContext().setObjectPropertyIndex(
            array.get(), i++, conversionResult.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);
    }

    return array;
}

static JSValueRef getProtobufMessageField(ProtobufArena& arena,
                                          JSProtobufMessage& message,
                                          const google::protobuf::FieldDescriptor* fieldDescriptor,
                                          JSFunctionNativeCallContext& callContext) {
    auto* field = message.getField(static_cast<Protobuf::FieldNumber>(fieldDescriptor->number()));

    if (field == nullptr) {
        return callContext.getContext().newUndefined();
    }

    if (fieldDescriptor->is_repeated()) {
        return getProtobufRepeatedField(arena, message, *field, fieldDescriptor, callContext);
    } else {
        auto* repeated = field->getRepeated();
        if (repeated != nullptr) {
            return getProtobufNonRepeatedField(arena, message, repeated->last(), fieldDescriptor, callContext);
        } else {
            return getProtobufNonRepeatedField(arena, message, *field, fieldDescriptor, callContext);
        }
    }
}

static JSValueRef setProtobufNonRepatedField(ProtobufArena& arena,
                                             JSProtobufMessage& message,
                                             const google::protobuf::FieldDescriptor* fieldDescriptor,
                                             Protobuf::Field& field,
                                             const JSValue& value,
                                             JSFunctionNativeCallContext& callContext) {
    auto& jsContext = callContext.getContext();
    switch (fieldDescriptor->type()) {
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE: {
            auto v = jsContext.valueToDouble(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setDouble(v);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_FLOAT: {
            auto v = jsContext.valueToDouble(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setFloat(static_cast<float>(v));
        } break;
        case google::protobuf::FieldDescriptor::TYPE_INT64: {
            auto v = jsContext.valueToLong(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setInt64(v.toInt64());
        } break;
        case google::protobuf::FieldDescriptor::TYPE_UINT64: {
            auto v = jsContext.valueToLong(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setUInt64(v.toUInt64());
        } break;
        case google::protobuf::FieldDescriptor::TYPE_INT32: {
            auto v = jsContext.valueToInt(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setInt32(v);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_FIXED64: {
            auto v = jsContext.valueToLong(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setFixed64(v.toUInt64());
        } break;
        case google::protobuf::FieldDescriptor::TYPE_FIXED32: {
            auto v = jsContext.valueToDouble(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setFixed32(static_cast<uint32_t>(v));
        } break;
        case google::protobuf::FieldDescriptor::TYPE_BOOL: {
            auto v = jsContext.valueToBool(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setBool(v);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_STRING: {
            auto v = jsContext.valueToStaticString(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setString(v.get());
        } break;
        case google::protobuf::FieldDescriptor::TYPE_GROUP:
            break;
        case google::protobuf::FieldDescriptor::TYPE_MESSAGE: {
            auto messageIndex = static_cast<size_t>(jsContext.valueToInt(value, callContext.getExceptionTracker()));
            CHECK_CALL_CONTEXT(callContext);

            auto* subMessage = arena.getMessage(messageIndex, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);

            field.setMessage(subMessage);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_BYTES: {
            auto typedArray = jsTypedArrayToValueTypedArray(
                jsContext, value, ReferenceInfoBuilder(), callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setTypedArray(typedArray.get());
        } break;
        case google::protobuf::FieldDescriptor::TYPE_UINT32: {
            auto v = jsContext.valueToDouble(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setUInt32(static_cast<uint32_t>(v));
        } break;
        case google::protobuf::FieldDescriptor::TYPE_ENUM: {
            auto v = jsContext.valueToInt(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setEnum(static_cast<Protobuf::EnumValue>(v));
        } break;
        case google::protobuf::FieldDescriptor::TYPE_SFIXED32: {
            auto v = jsContext.valueToInt(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setSFixed32(v);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_SFIXED64: {
            auto v = jsContext.valueToLong(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setSFixed64(v.toInt64());
        } break;
        case google::protobuf::FieldDescriptor::TYPE_SINT32: {
            auto v = jsContext.valueToInt(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setSInt32(v);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_SINT64: {
            auto v = jsContext.valueToLong(value, callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
            field.setSInt64(v.toInt64());
        } break;
    }

    return callContext.getContext().newUndefined();
}

static JSValueRef setProtobufFieldAsDefaultValue(ProtobufArena& arena,
                                                 JSProtobufMessage& message,
                                                 const google::protobuf::FieldDescriptor* fieldDescriptor,
                                                 Protobuf::Field& field,
                                                 JSFunctionNativeCallContext& callContext) {
    switch (fieldDescriptor->type()) {
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE: {
            field.setDouble(0.0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_FLOAT: {
            field.setFloat(0.0f);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_INT64: {
            field.setInt64(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_UINT64: {
            field.setUInt64(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_INT32: {
            field.setInt32(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_FIXED64: {
            field.setFixed64(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_FIXED32: {
            field.setFixed32(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_BOOL: {
            field.setBool(false);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_STRING: {
            field.setRaw(Protobuf::Field::Raw(nullptr, 0));
        } break;
        case google::protobuf::FieldDescriptor::TYPE_GROUP:
            break;
        case google::protobuf::FieldDescriptor::TYPE_MESSAGE: {
            field.setRaw(Protobuf::Field::Raw(nullptr, 0));
        } break;
        case google::protobuf::FieldDescriptor::TYPE_BYTES: {
            field.setRaw(Protobuf::Field::Raw(nullptr, 0));
        } break;
        case google::protobuf::FieldDescriptor::TYPE_UINT32: {
            field.setUInt32(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_ENUM: {
            field.setEnum(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_SFIXED32: {
            field.setSFixed32(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_SFIXED64: {
            field.setSFixed64(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_SINT32: {
            field.setSInt32(0);
        } break;
        case google::protobuf::FieldDescriptor::TYPE_SINT64: {
            field.setSInt64(0);
        } break;
    }

    return callContext.getContext().newUndefined();
}

static JSValueRef setProtobufRepeatedField(ProtobufArena& arena,
                                           JSProtobufMessage& message,
                                           const google::protobuf::FieldDescriptor* fieldDescriptor,
                                           Protobuf::Field& field,
                                           const JSValue& value,
                                           JSFunctionNativeCallContext& callContext) {
    auto* repeated = field.toRepeated();

    auto length = jsArrayGetLength(callContext.getContext(), value, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    repeated->clear();
    repeated->reserve(length);

    for (size_t i = 0; i < length; i++) {
        auto arrayItem =
            callContext.getContext().getObjectPropertyForIndex(value, i, callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        auto& itemField = repeated->append();

        setProtobufNonRepatedField(arena, message, fieldDescriptor, itemField, arrayItem.get(), callContext);
        CHECK_CALL_CONTEXT(callContext);
    }

    return callContext.getContext().newUndefined();
}

static void clearOneOfFields(JSProtobufMessage& message,
                             Protobuf::FieldNumber fieldNumber,
                             const google::protobuf::OneofDescriptor* oneOfDescriptor) {
    auto fieldNumberInt = static_cast<int>(fieldNumber);
    for (int i = 0; i < oneOfDescriptor->field_count(); i++) {
        const auto* fieldDescriptor = oneOfDescriptor->field(i);
        if (fieldDescriptor->number() != fieldNumberInt) {
            message.clearField(static_cast<Protobuf::FieldNumber>(fieldDescriptor->number()));
        }
    }
}

static JSValueRef setProtobufMessageField(ProtobufArena& arena,
                                          JSProtobufMessage& message,
                                          const google::protobuf::FieldDescriptor* fieldDescriptor,
                                          const JSValue& value,
                                          JSFunctionNativeCallContext& callContext) {
    auto fieldNumber = static_cast<Protobuf::FieldNumber>(fieldDescriptor->number());

    const auto* oneOf = fieldDescriptor->containing_oneof();
    if (oneOf != nullptr) {
        clearOneOfFields(message, fieldNumber, oneOf);

        auto& field = message.getOrCreateField(fieldNumber);
        field.setIsOneOf(true);

        if (callContext.getContext().isValueUndefined(value)) {
            return setProtobufFieldAsDefaultValue(arena, message, fieldDescriptor, field, callContext);
        }

        return setProtobufNonRepatedField(arena, message, fieldDescriptor, field, value, callContext);
    } else {
        if (callContext.getContext().isValueUndefined(value)) {
            message.clearField(fieldNumber);
            return callContext.getContext().newUndefined();
        }

        auto& field = message.getOrCreateField(fieldNumber);

        if (fieldDescriptor->is_repeated()) {
            return setProtobufRepeatedField(arena, message, fieldDescriptor, field, value, callContext);
        } else {
            return setProtobufNonRepatedField(arena, message, fieldDescriptor, field, value, callContext);
        }
    }
}

ProtobufModule::ProtobufModule(ResourceManager& resourceManager, const Ref<DispatchQueue>& workerQueue, ILogger& logger)
    : _resourcesManager(resourceManager), _workerQueue(workerQueue), _logger(logger) {}

ProtobufModule::~ProtobufModule() = default;

enum FieldModifier {
    FieldModifierNone = 0,
    FieldModifierRepeated = 1,
    FieldModifierMap = 2,
};

static FieldModifier fieldModifierForField(const google::protobuf::FieldDescriptor* field) {
    if (field->is_map()) {
        return FieldModifierMap;
    }
    if (field->is_repeated()) {
        return FieldModifierRepeated;
    }
    return FieldModifierNone;
}

static JSValueRef namespaceEntriesToJS(IJavaScriptContext& jsContext,
                                       JSExceptionTracker& exceptionTracker,
                                       const std::vector<ProtobufMessageFactory::NamespaceEntry>& namespaceEntries) {
    auto output = jsContext.newArray(namespaceEntries.size() * 2, exceptionTracker);
    if (!exceptionTracker) {
        return output;
    }

    size_t currentOutputIndex = 0;
    for (const auto& namespaceEntry : namespaceEntries) {
        auto type = static_cast<int32_t>(namespaceEntry.id) << 1 | (namespaceEntry.isMessage ? 1 : 0);

        auto typeJs = jsContext.newNumber(type);
        auto nameJs = jsContext.newStringUTF8(namespaceEntry.name, exceptionTracker);

        if (!exceptionTracker) {
            return output;
        }

        jsContext.setObjectPropertyIndex(output.get(), currentOutputIndex++, typeJs.get(), exceptionTracker);
        if (!exceptionTracker) {
            return output;
        }

        jsContext.setObjectPropertyIndex(output.get(), currentOutputIndex++, nameJs.get(), exceptionTracker);
        if (!exceptionTracker) {
            return output;
        }
    }

    return output;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::arenaCreateMessage(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArena(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageFactoryResult = getMessageFactory(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto descriptorIndex = getIndex(callContext, 2);
    CHECK_CALL_CONTEXT(callContext);

    auto result = arenaResult->createMessage(messageFactoryResult, descriptorIndex, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return toJSIndex(callContext, result);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::arenaDecodeMessage(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArena(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageFactoryResult = getMessageFactory(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto descriptorIndex = getIndex(callContext, 2);
    CHECK_CALL_CONTEXT(callContext);

    auto bytes = callContext.getParameterAsBytesView(3);
    CHECK_CALL_CONTEXT(callContext);

    auto result = arenaResult->decodeMessage(
        messageFactoryResult, descriptorIndex, bytes, false, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return toJSIndex(callContext, result);
}

JSValueRef ProtobufModule::arenaDecodeMessageAsync(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArenaUnsafe(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageFactoryResult = getMessageFactory(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto descriptorIndex = getIndex(callContext, 2);
    CHECK_CALL_CONTEXT(callContext);

    auto bytes = callContext.getParameterAsBytesView(3);
    CHECK_CALL_CONTEXT(callContext);

    auto callback = callContext.getParameterAsFunction(4);
    CHECK_CALL_CONTEXT(callContext);

    _workerQueue->async([arenaResult = std::move(arenaResult),
                         messageFactoryResult = std::move(messageFactoryResult),
                         descriptorIndex,
                         bytes = std::move(bytes),
                         callback = std::move(callback)]() {
        auto lock = arenaResult->lock();

        SimpleExceptionTracker exceptionTracker;
        auto result = arenaResult->decodeMessage(messageFactoryResult, descriptorIndex, bytes, true, exceptionTracker);

        if (exceptionTracker) {
            (*callback)({Value(static_cast<int32_t>(result)), Value::undefined()});
        } else {
            notifyAsyncCallbackError(callback, exceptionTracker.extractError());
        }
    });

    return callContext.getContext().newUndefined();
}

JSValueRef ProtobufModule::arenaDecodeMessageDebugJSONAsync(JSFunctionNativeCallContext& callContext) {
    if constexpr (ProtobufModule::areProtoDebugFeaturesEnabled()) {
        auto arenaResult = getArenaUnsafe(callContext, 0);
        CHECK_CALL_CONTEXT(callContext);

        auto messageFactoryResult = getMessageFactory(callContext, 1);
        CHECK_CALL_CONTEXT(callContext);

        auto descriptorIndex = getIndex(callContext, 2);
        CHECK_CALL_CONTEXT(callContext);

        auto json = callContext.getParameterAsStaticString(3);
        CHECK_CALL_CONTEXT(callContext);

        auto callback = callContext.getParameterAsFunction(4);
        CHECK_CALL_CONTEXT(callContext);

        _workerQueue->async([arenaResult = std::move(arenaResult),
                             messageFactoryResult = std::move(messageFactoryResult),
                             descriptorIndex,
                             json = std::move(json),
                             callback = std::move(callback)]() {
            auto utf8Storage = json->utf8Storage();
            auto lock = arenaResult->lock();

            SimpleExceptionTracker exceptionTracker;
            auto result = arenaResult->decodeMessageFromJSON(
                messageFactoryResult, descriptorIndex, utf8Storage.toStringView(), true, exceptionTracker);

            if (exceptionTracker) {
                (*callback)({Value(static_cast<int32_t>(result)), Value::undefined()});
            } else {
                notifyAsyncCallbackError(callback, exceptionTracker.extractError());
            }
        });

        return callContext.getContext().newUndefined();
    } else {
        return callContext.throwError(Error("Proto JSON is not enabled"));
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::arenaEncodeMessage(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArena(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageIndex = getIndex(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto encoded = arenaResult->encodeMessage(messageIndex, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return newTypedArrayFromBytesView(
        callContext.getContext(), TypedArrayType::Uint8Array, encoded, callContext.getExceptionTracker());
}

JSValueRef ProtobufModule::arenaEncodeMessageAsync(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArenaUnsafe(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageIndex = getIndex(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto callback = callContext.getParameterAsFunction(2);
    CHECK_CALL_CONTEXT(callContext);

    _workerQueue->async([arenaResult = std::move(arenaResult), messageIndex, callback = std::move(callback)]() {
        auto lock = arenaResult->lock();
        SimpleExceptionTracker exceptionTracker;

        auto encoded = arenaResult->encodeMessage(messageIndex, exceptionTracker);

        if (!exceptionTracker) {
            notifyAsyncCallbackError(callback, exceptionTracker.extractError());
            return;
        }

        (*callback)({Value(makeShared<ValueTypedArray>(TypedArrayType::Uint8Array, encoded)), Value::undefined()});
    });

    return callContext.getContext().newUndefined();
}

static std::vector<size_t> getMessageIndexes(JSFunctionNativeCallContext& callContext, size_t parameterIndex) {
    std::vector<size_t> output;

    auto array = callContext.getParameter(parameterIndex);
    auto length = jsArrayGetLength(callContext.getContext(), array, callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return output;
    }

    output.reserve(length);

    for (size_t i = 0; i < length; i++) {
        auto property = callContext.getContext().getObjectPropertyForIndex(array, i, callContext.getExceptionTracker());
        if (!callContext.getExceptionTracker()) {
            return output;
        }
        output.emplace_back(static_cast<size_t>(
            callContext.getContext().valueToInt(property.get(), callContext.getExceptionTracker())));
        if (!callContext.getExceptionTracker()) {
            return output;
        }
    }

    return output;
}

JSValueRef ProtobufModule::arenaBatchEncodeMessageAsync(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArenaUnsafe(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageIndexes = getMessageIndexes(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto callback = callContext.getParameterAsFunction(2);
    CHECK_CALL_CONTEXT(callContext);

    _workerQueue->async([arenaResult = std::move(arenaResult),
                         messageIndexes = std::move(messageIndexes),
                         callback = std::move(callback)]() {
        VALDI_TRACE("Protobuf.encodeMessageBatch");
        auto lock = arenaResult->lock();

        auto length = messageIndexes.size();
        auto output = ValueArray::make(length);

        SimpleExceptionTracker exceptionTracker;

        for (size_t i = 0; i < length; i++) {
            auto messageIndex = messageIndexes[i];

            auto encoded = arenaResult->encodeMessage(messageIndex, exceptionTracker);
            if (!exceptionTracker) {
                notifyAsyncCallbackError(callback, exceptionTracker.extractError());
                return;
            }

            (*output)[i] = Value(makeShared<ValueTypedArray>(TypedArrayType::Uint8Array, encoded));
        }

        (*callback)({Value(output), Value::undefined()});
    });

    return callContext.getContext().newUndefined();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::arenaEncodeMessageToJSON(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArena(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageIndex = getIndex(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto jsonPrintOptions = callContext.getParameterAsInt(2);
    CHECK_CALL_CONTEXT(callContext);

    Protobuf::JSONPrintOptions resolvedPrintOptions;
    resolvedPrintOptions.pretty = (jsonPrintOptions & 1) != 0;
    resolvedPrintOptions.alwaysPrintPrimitiveFields = ((jsonPrintOptions >> 1) & 1) != 0;
    resolvedPrintOptions.alwaysPrintEnumsAsInts = ((jsonPrintOptions >> 2) & 1) != 0;

    auto json = arenaResult->messageToJSON(messageIndex, resolvedPrintOptions, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return callContext.getContext().newStringUTF8(json, callContext.getExceptionTracker());
}

static Error onInvalidFieldsDescriptor() {
    return Error("Invalid fields array");
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::arenaSetMessageField(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArena(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageIndex = getIndex(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto fieldIndex = getIndex(callContext, 2);
    CHECK_CALL_CONTEXT(callContext);

    auto message = arenaResult->getMessage(messageIndex, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    const auto* descriptor = message->getDescriptor();

    auto fieldCount = descriptor->field_count();

    if (fieldIndex >= static_cast<size_t>(fieldCount)) {
        return callContext.throwError(onInvalidFieldsDescriptor());
    }

    return setProtobufMessageField(*arenaResult,
                                   *message,
                                   descriptor->field(static_cast<int>(fieldIndex)),
                                   callContext.getParameter(3),
                                   callContext);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::arenaGetMessageFields(JSFunctionNativeCallContext& callContext) {
    auto arena = getArena(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageIndex = getIndex(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto* message = arena->getMessage(messageIndex, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    const auto* descriptor = message->getDescriptor();

    return callContext.getContext().newArrayWithValues(
        static_cast<size_t>(descriptor->field_count()), callContext.getExceptionTracker(), [&](size_t i) {
            const auto* fieldDescriptor = descriptor->field(static_cast<int>(i));

            return getProtobufMessageField(*arena, *message, fieldDescriptor, callContext);
        });
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::arenaCopyMessage(JSFunctionNativeCallContext& callContext) {
    auto arenaResult = getArena(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageIndex = getIndex(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    auto otherArenaResult = getArena(callContext, 2);
    CHECK_CALL_CONTEXT(callContext);

    auto copyIndex = arenaResult->copyMessage(*otherArenaResult, messageIndex, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return toJSIndex(callContext, copyIndex);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::createArena(JSFunctionNativeCallContext& callContext) {
    auto eagerDecoding = callContext.getParameterAsBool(0);
    CHECK_CALL_CONTEXT(callContext);

    auto includeAllFieldsDuringEncoding = callContext.getParameterAsBool(1);
    CHECK_CALL_CONTEXT(callContext);

    auto arena = Valdi::makeShared<ProtobufArena>(eagerDecoding, includeAllFieldsDuringEncoding);

    return makeWrappedObject(callContext.getContext(), arena, callContext.getExceptionTracker(), true);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::getFieldsForMessageDescriptor(JSFunctionNativeCallContext& callContext) {
    auto messageFactory = getMessageFactory(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto descriptorIndex = getIndex(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    // Hold the factory lock for the entire sequence of factory calls so they complete atomically.
    auto factoryLock = messageFactory->lock();

    const auto* descriptor = messageFactory->getDescriptorAtIndex(descriptorIndex, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    auto& jsContext = callContext.getContext();

    auto nameKey = jsContext.newPropertyName("name");
    auto indexKey = jsContext.newPropertyName("index");
    auto typeKey = jsContext.newPropertyName("type");
    auto cppTypeKey = jsContext.newPropertyName("cppType");
    auto modifierKey = jsContext.newPropertyName("modifier");
    auto otherDescriptorIndexKey = jsContext.newPropertyName("otherDescriptorIndex");
    auto oneOfFieldIndexKey = jsContext.newPropertyName("oneOfFieldIndex");
    auto enumValuesKey = jsContext.newPropertyName("enumValues");

    auto outFields =
        jsContext.newArray(static_cast<size_t>(descriptor->field_count()), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    for (int i = 0; i < descriptor->field_count(); i++) {
        const auto* field = descriptor->field(i);

        auto outField = jsContext.newObject(callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        auto fieldName = jsContext.newStringUTF8(field->camelcase_name(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        auto fieldIndex = jsContext.newNumber(static_cast<int32_t>(field->index()));
        auto fieldType = jsContext.newNumber(static_cast<int32_t>(field->type()));
        auto fieldCppType = jsContext.newNumber(static_cast<int32_t>(field->cpp_type()));
        auto fieldModifier = jsContext.newNumber(static_cast<int32_t>(fieldModifierForField(field)));

        jsContext.setObjectProperty(outField.get(), nameKey.get(), fieldName.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);
        jsContext.setObjectProperty(
            outField.get(), indexKey.get(), fieldIndex.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);
        jsContext.setObjectProperty(outField.get(), typeKey.get(), fieldType.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);
        jsContext.setObjectProperty(
            outField.get(), cppTypeKey.get(), fieldCppType.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);
        jsContext.setObjectProperty(
            outField.get(), modifierKey.get(), fieldModifier.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        // Populate the other message index parameter
        if (field->type() == google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE) {
            auto matchingPrototype = messageFactory->getMessagePrototypeIndexForDescriptor(
                field->message_type(), callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);

            auto fieldOtherDescriptorIndex = jsContext.newNumber(static_cast<int32_t>(matchingPrototype));
            jsContext.setObjectProperty(outField.get(),
                                        otherDescriptorIndexKey.get(),
                                        fieldOtherDescriptorIndex.get(),
                                        callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
        }

        // Populate the other one-of field indexes parameter
        const auto* containingOneof = field->containing_oneof();
        if (containingOneof != nullptr) {
            auto fieldOneOfFieldIndex = jsContext.newNumber(static_cast<int32_t>(containingOneof->index()));

            jsContext.setObjectProperty(outField.get(),
                                        oneOfFieldIndexKey.get(),
                                        fieldOneOfFieldIndex.get(),
                                        callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
        }

        // Populate the known enum values
        if (field->type() == google::protobuf::FieldDescriptor::Type::TYPE_ENUM) {
            const auto* enumDescriptor = field->enum_type();

            auto enumValues = jsContext.newArray(static_cast<size_t>(enumDescriptor->value_count()),
                                                 callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);

            for (int j = 0; j < enumDescriptor->value_count(); j++) {
                auto enumValue = jsContext.newNumber(static_cast<int32_t>(enumDescriptor->value(j)->number()));

                jsContext.setObjectPropertyIndex(
                    enumValues.get(), static_cast<size_t>(j), enumValue.get(), callContext.getExceptionTracker());
                CHECK_CALL_CONTEXT(callContext);
            }

            jsContext.setObjectProperty(
                outField.get(), enumValuesKey.get(), enumValues.get(), callContext.getExceptionTracker());
            CHECK_CALL_CONTEXT(callContext);
        }

        jsContext.setObjectPropertyIndex(
            outFields.get(), static_cast<size_t>(i), outField.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);
    }

    return outFields;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::getNamespaceEntries(JSFunctionNativeCallContext& callContext) {
    auto messageFactory = getMessageFactory(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto namespaceId = getIndex(callContext, 1);
    CHECK_CALL_CONTEXT(callContext);

    std::vector<ProtobufMessageFactory::NamespaceEntry> namespaceEnties;
    {
        auto factoryLock = messageFactory->lock();
        namespaceEnties = messageFactory->getNamespaceEntriesForId(namespaceId, callContext.getExceptionTracker());
    }
    CHECK_CALL_CONTEXT(callContext);

    return namespaceEntriesToJS(callContext.getContext(), callContext.getExceptionTracker(), namespaceEnties);
}

JSValueRef ProtobufModule::doLoadMessagesFromFactory(const Ref<ProtobufMessageFactory>& messageFactory,
                                                     JSFunctionNativeCallContext& callContext) {
    std::vector<std::string> descriptorNames;
    std::vector<ProtobufMessageFactory::NamespaceEntry> rootNamespaceEntries;
    {
        auto factoryLock = messageFactory->lock();
        descriptorNames = messageFactory->getDescriptorNames();
        rootNamespaceEntries = messageFactory->getRootNamespaceEntries();
    }

    // Step 2: We build a description of all the registered messages and fields along with their indexes
    // in which they can be reached in the message factory.

    auto& jsContext = callContext.getContext();

    auto outDescriptorNames = jsContext.newArray(descriptorNames.size(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    size_t messageIndex = 0;
    for (const auto& descriptorName : descriptorNames) {
        auto messageFullName = jsContext.newStringUTF8(descriptorName, callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        jsContext.setObjectPropertyIndex(
            outDescriptorNames.get(), messageIndex, messageFullName.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        messageIndex++;
    }

    auto rootNamespacesJS =
        namespaceEntriesToJS(callContext.getContext(), callContext.getExceptionTracker(), rootNamespaceEntries);
    CHECK_CALL_CONTEXT(callContext);

    // Step 3: Send back the result

    auto loadMessageResult = jsContext.newObject(callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    auto messageFactoryValue = jsContext.newWrappedObject(messageFactory, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    jsContext.setObjectProperty(
        loadMessageResult.get(), "factory", messageFactoryValue.get(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    jsContext.setObjectProperty(
        loadMessageResult.get(), "descriptorNames", outDescriptorNames.get(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    jsContext.setObjectProperty(
        loadMessageResult.get(), "rootNamespaceEntries", rootNamespacesJS.get(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return loadMessageResult;
}

Ref<ProtobufMessageFactory> ProtobufModule::getMessageFactoryAtPath(ResourceManager& resourceManager,
                                                                    const StringBox& importPath,
                                                                    ExceptionTracker& exceptionTracker) {
    auto resourceIdResult = JavaScriptPathResolver::resolveResourceId(importPath);
    if (!resourceIdResult) {
        exceptionTracker.onError(resourceIdResult.moveError());
        return nullptr;
    }

    auto bundle = resourceManager.getBundle(resourceIdResult.value().bundleName);

    // Acquire a lock so that we can getOrCreate the ProtobufMessageFactory
    auto lock = bundle->lock();

    auto entryResult = bundle->getResourceContent(resourceIdResult.value().resourcePath);
    if (!entryResult) {
        exceptionTracker.onError(entryResult.moveError());
        return nullptr;
    }

    auto entry = entryResult.moveValue();

    auto messageFactory = castOrNull<ProtobufMessageFactory>(entry.processed);
    if (messageFactory == nullptr) {
        VALDI_TRACE("Protobuf.initializeMessageFactory");
        // Step 1: We parse the protobuf definitions and load them inside a message factory
        messageFactory = Valdi::makeShared<ProtobufMessageFactory>();
        messageFactory->load(entry.raw, exceptionTracker);
        if (!exceptionTracker) {
            return nullptr;
        }
        bundle->setResourceContent(resourceIdResult.value().resourcePath, BundleResourceContent(messageFactory));
    }

    return messageFactory;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef ProtobufModule::loadMessages(JSFunctionNativeCallContext& callContext) {
    VALDI_TRACE("Protobuf.loadMessages");
    auto pathResult = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageFactory =
        ProtobufModule::getMessageFactoryAtPath(_resourcesManager, pathResult, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return doLoadMessagesFromFactory(messageFactory, callContext);
}

JSValueRef ProtobufModule::loadMessagesFromProtoFileContent(JSFunctionNativeCallContext& callContext) {
    if constexpr (ProtobufModule::areProtoDebugFeaturesEnabled()) {
        auto filename = callContext.getParameterAsStaticString(0)->toStdString();
        auto protoFileContent = callContext.getParameterAsStaticString(1);
        CHECK_CALL_CONTEXT(callContext);
        auto utf8Storage = protoFileContent->utf8Storage();
        auto messageFactory = Valdi::makeShared<ProtobufMessageFactory>();

        if (!messageFactory->parseAndLoad(filename, utf8Storage.toStringView(), callContext.getExceptionTracker())) {
            return JSValueRef();
        }

        return doLoadMessagesFromFactory(messageFactory, callContext);
    } else {
        return callContext.throwError(Error("Proto parsing not enabled"));
    }
}

} // namespace Valdi
