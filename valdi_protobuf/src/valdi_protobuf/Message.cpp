#include "valdi_protobuf/Message.hpp"
#include "utils/encoding/Base64Utils.hpp"
#include "utils/platform/BuildOptions.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include <algorithm>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/parse_context.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/util/type_resolver_util.h>
#include <google/protobuf/wire_format_lite.h>

#include <fmt/format.h>

namespace Valdi::Protobuf {

class DefaultMessageFactory : public IMessageFactory {
public:
    ~DefaultMessageFactory() override = default;

    Ref<Message> newMessage(const google::protobuf::Descriptor* descriptor, const Ref<RefCountable>& dataSource) final {
        return makeShared<Message>(descriptor, dataSource);
    }
};

using WireType = google::protobuf::internal::WireFormatLite::WireType;

struct JSONHelper {
    std::string output;
    google::protobuf::io::ArrayInputStream inputStream;
    google::protobuf::io::StringOutputStream outputStream;
    google::protobuf::util::TypeResolver* typeResolver;
    std::string typeUrl;

    JSONHelper(const google::protobuf::Descriptor* descriptor, const Byte* input, size_t inputLength)
        : inputStream(input, static_cast<int>(inputLength)), outputStream(&output) {
        std::string typeUrlPrefix = "type.googleapis.com";
        const auto* pool = descriptor->file()->pool();
        typeResolver = google::protobuf::util::NewTypeResolverForDescriptorPool(typeUrlPrefix, pool);
        typeUrl = fmt::format("{}/{}", typeUrlPrefix, descriptor->full_name());
    }

    ~JSONHelper() {
        delete typeResolver;
    }
};

Message::Message() = default;
Message::Message(const google::protobuf::Descriptor* descriptor, const Ref<RefCountable>& dataSource)
    : _descriptor(descriptor), _dataSource(dataSource) {}

Message::~Message() = default;

BytesView Message::encode(bool includeEmptyFields) {
    auto length = encodedByteSize(includeEmptyFields);
    auto byteBuffer = makeShared<ByteBuffer>();
    byteBuffer->resize(length);

    encode(includeEmptyFields, byteBuffer->begin(), byteBuffer->end());

    return byteBuffer->toBytesView();
}

Byte* Message::encode(bool includeEmptyFields, Byte* bufferStart, Byte* bufferEnd) const {
    _fieldMap.forEachSorted([&](const FieldMap::Entry& entry) {
        bufferStart = entry.value->write(entry.number, includeEmptyFields, includeEmptyFields, bufferStart, bufferEnd);
    });
    return bufferStart;
}

size_t Message::encodedByteSize(bool includeEmptyFields) {
    size_t byteSize = 0;
    _fieldMap.forEach([&](const FieldMap::Entry& entry) {
        byteSize += entry.value->byteSize(entry.number, includeEmptyFields, includeEmptyFields);
    });

    _cachedEncodedByteSize = byteSize;

    return byteSize;
}

size_t Message::getCachedEncodedByteSize() const {
    return _cachedEncodedByteSize;
}

void Message::appendField(FieldNumber fieldNumber, Field field) {
    auto& it = getOrCreateField(fieldNumber);
    if (it.isUnset()) {
        it = std::move(field);
    } else {
        it.append(std::move(field));
    }
}

void Message::clearAllFields() {
    _fieldMap.clear();
}

void Message::clearField(FieldNumber fieldNumber) {
    _fieldMap.erase(fieldNumber);
}

std::vector<FieldNumber> Message::sortedFieldNumbers() const {
    return _fieldMap.sortedFieldNumbers();
}

std::optional<FieldNumber> Message::getFieldNumberForName(std::string_view fieldName) const {
    if (_descriptor == nullptr) {
        return std::nullopt;
    }

    const auto* fieldDescriptor = _descriptor->FindFieldByName(std::string(fieldName));
    if (fieldDescriptor == nullptr) {
        return std::nullopt;
    }
    return static_cast<size_t>(fieldDescriptor->number());
}

Field* Message::getFieldByName(std::string_view fieldName) {
    auto fieldNumber = getFieldNumberForName(fieldName);
    if (!fieldNumber) {
        return nullptr;
    }
    return getField(fieldNumber.value());
}

const Field* Message::getFieldByName(std::string_view fieldName) const {
    auto fieldNumber = getFieldNumberForName(fieldName);
    if (!fieldNumber) {
        return nullptr;
    }
    return getField(fieldNumber.value());
}

Field* Message::getField(FieldNumber fieldNumber) {
    return _fieldMap.find(fieldNumber);
}

Field& Message::getOrCreateField(FieldNumber fieldNumber) {
    return _fieldMap[fieldNumber];
}

const Field* Message::getField(FieldNumber fieldNumber) const {
    return _fieldMap.find(fieldNumber);
}

std::string_view Message::getFieldAsString(FieldNumber fieldNumber) const {
    const auto* field = getField(fieldNumber);
    if (field == nullptr) {
        return std::string_view();
    }

    auto raw = field->getRaw();
    return std::string_view(reinterpret_cast<const char*>(raw.data), raw.length);
}

template<typename T, typename F>
T toFieldIterator(F field) {
    if (field == nullptr) {
        return T(nullptr, nullptr);
    }
    auto* repeatedField = field->getRepeated();
    if (repeatedField != nullptr) {
        return T(repeatedField->begin(), repeatedField->end());
    } else {
        return T(field, field + 1);
    }
}

RepeatedFieldIterator Message::getFieldIterator(FieldNumber fieldNumber) {
    return toFieldIterator<RepeatedFieldIterator>(getField(fieldNumber));
}

RepeatedFieldConstIterator Message::getFieldIterator(FieldNumber fieldNumber) const {
    return toFieldIterator<RepeatedFieldConstIterator>(getField(fieldNumber));
}

RepeatedFieldIterator Message::getFieldIteratorForName(std::string_view fieldName) {
    return toFieldIterator<RepeatedFieldIterator>(getFieldByName(fieldName));
}

RepeatedFieldConstIterator Message::getFieldIteratorForName(std::string_view fieldName) const {
    return toFieldIterator<RepeatedFieldConstIterator>(getFieldByName(fieldName));
}

const Ref<RefCountable>& Message::getDataSource() const {
    return _dataSource;
}

const google::protobuf::Descriptor* Message::getDescriptor() const {
    return _descriptor;
}

std::string Message::toJSON(const JSONPrintOptions& options, ExceptionTracker& exceptionTracker) {
    if (_descriptor == nullptr) {
        exceptionTracker.onError("Cannot convert to JSON without a descriptor");
        return "";
    }

    auto encoded = encode(options.alwaysPrintPrimitiveFields);

    JSONHelper jsonHelper(_descriptor, encoded.data(), encoded.size());

    auto outputOptions = google::protobuf::util::JsonPrintOptions();
    outputOptions.add_whitespace = options.pretty;
    outputOptions.always_print_fields_with_no_presence = options.alwaysPrintPrimitiveFields;
    outputOptions.always_print_enums_as_ints = options.alwaysPrintEnumsAsInts;

    auto result = google::protobuf::util::BinaryToJsonStream(
        jsonHelper.typeResolver, jsonHelper.typeUrl, &jsonHelper.inputStream, &jsonHelper.outputStream, outputOptions);

    if (!result.ok()) {
        exceptionTracker.onError(result.message());
        return "";
    }

    return jsonHelper.output;
}

bool Message::decodeFromJSON(std::string_view json, ExceptionTracker& exceptionTracker) {
    if (_descriptor == nullptr) {
        exceptionTracker.onError("Cannot parse from JSON without a descriptor");
        return false;
    }

    JSONHelper jsonHelper(_descriptor, reinterpret_cast<const Byte*>(json.data()), json.size());

    auto result = google::protobuf::util::JsonToBinaryStream(jsonHelper.typeResolver,
                                                             jsonHelper.typeUrl,
                                                             &jsonHelper.inputStream,
                                                             &jsonHelper.outputStream,
                                                             google::protobuf::util::JsonParseOptions());
    if (!result.ok()) {
        exceptionTracker.onError(result.message());
        return false;
    }

    auto buffer = makeShared<ByteBuffer>();
    buffer->set(reinterpret_cast<const Byte*>(jsonHelper.output.data()),
                reinterpret_cast<const Byte*>(jsonHelper.output.data() + jsonHelper.output.size()));
    _dataSource = buffer;

    return decode(buffer->data(), buffer->size(), exceptionTracker);
}

Ref<Message> Message::clone() const {
    auto out = makeShared<Message>(_descriptor, _dataSource);
    out->_fieldMap = _fieldMap;
    _fieldMap.forEach([&](const auto& it) { out->_fieldMap[it.number] = it.value->clone(); });

    return out;
}

Ref<Message> Message::parse(const BytesView& bytes,
                            const google::protobuf::Descriptor* descriptor,
                            ExceptionTracker& exceptionTracker) {
    auto out = makeShared<Message>(descriptor, bytes.getSource());

    if (!out->decode(bytes.data(), bytes.size(), exceptionTracker)) {
        return nullptr;
    }

    return out;
}

Result<Ref<Message>> Message::parse(const BytesView& bytes, const google::protobuf::Descriptor* descriptor) {
    SimpleExceptionTracker exceptionTracker;
    return exceptionTracker.toResult(parse(bytes, descriptor, exceptionTracker));
}

Ref<Message> Message::parseFromJSON(std::string_view json,
                                    const google::protobuf::Descriptor* descriptor,
                                    ExceptionTracker& exceptionTracker) {
    auto out = makeShared<Message>(descriptor, nullptr);

    if (!out->decodeFromJSON(json, exceptionTracker)) {
        return nullptr;
    }

    return out;
}

Result<Ref<Message>> Message::parseFromJSON(std::string_view json, const google::protobuf::Descriptor* descriptor) {
    SimpleExceptionTracker exceptionTracker;
    return exceptionTracker.toResult(parseFromJSON(json, descriptor, exceptionTracker));
}

static Valdi::ILogger* kLogger = nullptr;

void Message::setLogger(Valdi::ILogger* logger) {
    Valdi::unsafeRelease(kLogger);
    kLogger = Valdi::unsafeRetain(logger);
}

bool Message::onDecodeError(
    std::string_view message, int fieldNumber, const Byte* data, size_t length, ExceptionTracker& exceptionTracker) {
    auto descriptorName = _descriptor != nullptr ? _descriptor->full_name() : "<Unknown>";
    std::string errorMessage =
        fmt::format("In message type '{}' and field number {}: {}", descriptorName, fieldNumber, message);

    if (snap::kIsDevBuild || snap::kIsGoldBuild) {
        auto logger = Valdi::strongSmallRef(kLogger);
        if (logger != nullptr) {
            VALDI_ERROR(*logger, "Message failed to decode: {}", errorMessage);
            VALDI_ERROR(
                *logger, "{} payload content: {}", descriptorName, snap::utils::encoding::binaryToBase64(data, length));
        }
    }

    exceptionTracker.onError(std::move(errorMessage));
    return false;
}

bool Message::decode(const Byte* data, size_t length, ExceptionTracker& exceptionTracker) {
    google::protobuf::io::CodedInputStream inputStream(data, static_cast<int>(length));

    for (;;) {
        auto tag = inputStream.ReadTag();
        if (tag == 0) {
            break;
        }
        auto wireType = static_cast<WireType>(tag & 0x7);
        auto fieldNumber = static_cast<int>(tag >> 3);

        switch (wireType) {
            case WireType::WIRETYPE_VARINT:
                uint64_t varint;
                if (!inputStream.ReadVarint64(&varint)) {
                    return onDecodeError("Unable to read varint", fieldNumber, data, length, exceptionTracker);
                }

                appendField(fieldNumber, Field::varint(varint));
                break;
            case WireType::WIRETYPE_FIXED64:
                uint64_t fixed64;
                if (!inputStream.ReadLittleEndian64(&fixed64)) {
                    return onDecodeError("Unable to read fixed64", fieldNumber, data, length, exceptionTracker);
                }

                appendField(fieldNumber, Field::fixed64(fixed64));
                break;
            case WireType::WIRETYPE_LENGTH_DELIMITED: {
                uint32_t innerLength;
                if (!inputStream.ReadVarint32(&innerLength)) {
                    return onDecodeError("Unable to read varint32", fieldNumber, data, length, exceptionTracker);
                }

                if (innerLength > 0) {
                    const void* dataPtr;
                    int sizeToEndOfBuffer;
                    if (!inputStream.GetDirectBufferPointer(&dataPtr, &sizeToEndOfBuffer)) {
                        return onDecodeError(
                            "Unable to get direct buffer", fieldNumber, data, length, exceptionTracker);
                    }

                    if (!inputStream.Skip(static_cast<int>(innerLength))) {
                        return onDecodeError(
                            "Out of bounds length delimited", fieldNumber, data, length, exceptionTracker);
                    }

                    appendField(fieldNumber, Field::raw(reinterpret_cast<const Byte*>(dataPtr), innerLength));
                } else {
                    appendField(fieldNumber, Field::raw(&data[static_cast<size_t>(inputStream.CurrentPosition())], 0));
                }
            } break;
            case WireType::WIRETYPE_START_GROUP:
            case WireType::WIRETYPE_END_GROUP:
                return onDecodeError("group wiretype are not supported", fieldNumber, data, length, exceptionTracker);
            case WireType::WIRETYPE_FIXED32:
                uint32_t fixed32;
                if (!inputStream.ReadLittleEndian32(&fixed32)) {
                    return onDecodeError("Unable to read fixed32", fieldNumber, data, length, exceptionTracker);
                }

                appendField(fieldNumber, Field::fixed32(fixed32));
                break;
            default:
                return onDecodeError("Unsupported wiretype", fieldNumber, data, length, exceptionTracker);
        }
    }

    if (!inputStream.ExpectAtEnd()) {
        exceptionTracker.onError(Error("Invalid end of stream"));
        return false;
    }

    populateFieldFlags();

    return true;
}

bool Message::populateFieldFlags() {
    if (_descriptor == nullptr) {
        return false;
    }
    auto oneOfCount = _descriptor->oneof_decl_count();
    for (int i = 0; i < oneOfCount; i++) {
        const auto* oneOfFieldDescriptor = _descriptor->oneof_decl(i);
        auto fieldsCount = oneOfFieldDescriptor->field_count();
        for (int j = 0; j < fieldsCount; j++) {
            const auto* field = oneOfFieldDescriptor->field(j);
            auto* fieldValue = getField(static_cast<Protobuf::FieldNumber>(field->number()));
            if (fieldValue != nullptr) {
                fieldValue->setIsOneOf(true);
            }
        }
    }

    return true;
}

static bool onPopulateFieldError(const google::protobuf::FieldDescriptor& fieldDescriptor,
                                 std::string_view errorMessage,
                                 ExceptionTracker& exceptionTracker) {
    exceptionTracker.onError(fmt::format("{} on field {}", errorMessage, fieldDescriptor.full_name()));
    return false;
}

bool Message::postprocessForField(const google::protobuf::FieldDescriptor& fieldDescriptor,
                                  Field& fieldValue,
                                  bool recursive,
                                  IMessageFactory& messageFactory,
                                  ExceptionTracker& exceptionTracker) {
    for (;;) {
        auto internalFieldType = fieldValue.getInternalType();

        if (internalFieldType == Field::InternalType::Raw) {
            auto raw = fieldValue.getRaw();
            auto childMessage = messageFactory.newMessage(fieldDescriptor.message_type(), _dataSource);
            if (!childMessage->decode(raw.data, raw.length, exceptionTracker)) {
                return onPopulateFieldError(fieldDescriptor, "Failed to decode", exceptionTracker);
            }

            if (recursive) {
                if (!childMessage->postprocess(true, messageFactory, exceptionTracker)) {
                    return onPopulateFieldError(fieldDescriptor, "Failed to populate child messages", exceptionTracker);
                }
            }

            fieldValue.setMessage(childMessage.get());
        } else if (internalFieldType == Field::InternalType::Ref) {
            const auto* repeated = fieldValue.getRepeated();
            if (repeated != nullptr) {
                // If we parsed the field as a repeated field,
                // transform it into a single field
                fieldValue = repeated->last();
                // Re-try again now that we have made the field non repeated
                continue;
            }

            auto* childMessage = fieldValue.getMessage();
            if (childMessage == nullptr) {
                return onPopulateFieldError(fieldDescriptor, "Expected message type", exceptionTracker);
            }

            if (recursive) {
                if (!childMessage->postprocess(true, exceptionTracker)) {
                    return onPopulateFieldError(fieldDescriptor, "Failed to populate child messages", exceptionTracker);
                }
            }
        } else {
            return onPopulateFieldError(fieldDescriptor, "Invalid decoded field type", exceptionTracker);
        }

        return true;
    }
}

bool Message::postprocess(bool recursive, ExceptionTracker& exceptionTracker) {
    DefaultMessageFactory messageFactory;
    return postprocess(recursive, messageFactory, exceptionTracker);
}

bool Message::postprocess(bool recursive, IMessageFactory& messageFactory, ExceptionTracker& exceptionTracker) {
    if (_descriptor == nullptr) {
        exceptionTracker.onError("Cannot postprocess Message without a Descriptor set");
        return false;
    }

    auto fieldCount = _descriptor->field_count();
    for (int i = 0; i < fieldCount; i++) {
        const auto& fieldDescriptor = *_descriptor->field(i);
        auto isRepeated = fieldDescriptor.is_repeated();
        auto isMessage = fieldDescriptor.cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE;
        if (!(isRepeated || isMessage)) {
            // Standard field that doesn't need to be processed
            continue;
        }

        auto* it = _fieldMap.find(static_cast<size_t>(fieldDescriptor.number()));
        if (it == nullptr) {
            continue;
        }

        auto& fieldValue = *it;

        if (isRepeated) {
            auto* repeated = fieldValue.toRepeated(fieldDescriptor);

            if (isMessage) {
                for (auto& repeatedFieldValue : *repeated) {
                    if (!postprocessForField(
                            fieldDescriptor, repeatedFieldValue, recursive, messageFactory, exceptionTracker)) {
                        return false;
                    }
                }
            }
        } else {
            if (!postprocessForField(fieldDescriptor, fieldValue, recursive, messageFactory, exceptionTracker)) {
                return false;
            }
        }
    }

    return true;
}

} // namespace Valdi::Protobuf
