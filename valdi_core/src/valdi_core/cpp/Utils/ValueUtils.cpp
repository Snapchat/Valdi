//
//  ValueUtils.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/27/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/cpp/Utils/ValueUtils.hpp"
#include "valdi_core/cpp/Utils/JSONReader.hpp"
#include "valdi_core/cpp/Utils/JSONWriter.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"

#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/TextParser.hpp"
#include "valdi_core/cpp/Utils/ValueConvertible.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"

#include <fmt/format.h>

#include "utils/encoding/Base64Utils.hpp"
#include "json/reader.h"
#include "json/writer.h"

namespace Valdi {

Value deserializeValue(const Byte* data, size_t size) {
    auto str = StringCache::getGlobal().makeString(reinterpret_cast<const char*>(data), size);

    Value out;

    for (const auto& line : str.split('\n')) {
        auto index = line.indexOf('=');
        if (index) {
            auto keyValue = line.split(*index);
            out.setMapValue(keyValue.first, Value(keyValue.second));
        }
    }

    return out;
}

Ref<ByteBuffer> serializeValue(const Value& value) {
    auto out = makeShared<ByteBuffer>();

    const auto* map = value.getMap();
    if (map != nullptr) {
        auto keys = value.sortedMapKeys();

        for (const auto& key : keys) {
            auto value = map->find(key)->second.toStringBox();

            auto keyStrView = key.toStringView();
            auto valueStrView = value.toStringView();

            out->append(keyStrView.begin(), keyStrView.end());
            out->append('=');
            out->append(valueStrView.begin(), valueStrView.end());
            out->append('\n');
        }
    }

    return out;
}

static void writeTypedObjectToJSONWriter(const ValueTypedObject& typedObject, JSONWriter& writer) {
    writer.writeBeginObject();

    auto isFirst = true;
    for (const auto& it : typedObject) {
        if (!isFirst) {
            writer.writeComma();
        }
        isFirst = false;
        writer.writeProperty(it.name.toStringView());
        writeValueToJSONWriter(it.value, writer);
    }

    writer.writeEndObject();
}

Ref<ByteBuffer> valueToJson(const Value& value) {
    auto out = makeShared<ByteBuffer>();
    JSONWriter writer(*out);
    writeValueToJSONWriter(value, writer);
    return out;
}

std::string valueToJsonString(const Value& value) {
    auto json = valueToJson(value);
    auto stringView = std::string_view(reinterpret_cast<const char*>(json->data()), json->size());
    return std::string(stringView);
}

void writeValueToJSONWriter(const Value& value, JSONWriter& writer) {
    switch (value.getType()) {
        case ValueType::Null:
        case ValueType::Undefined:
            writer.writeNull();
            break;
        case ValueType::InternedString:
            writer.writeString(value.toStringBox().toStringView());
            break;
        case ValueType::StaticString: {
            auto utf8Storage = value.getStaticString()->utf8Storage();
            writer.writeString(utf8Storage.toStringView());
        } break;
        case ValueType::Error:
            writer.writeString(value.toStringBox().toStringView());
            break;
        case ValueType::TypedObject:
            return writeTypedObjectToJSONWriter(*value.getTypedObject(), writer);
        case ValueType::ProxyTypedObject:
            return writeTypedObjectToJSONWriter(*value.getProxyObject()->getTypedObject(), writer);
        case ValueType::ValdiObject: {
            auto* valueConvertible = dynamic_cast<ValueConvertible*>(value.getValdiObject().get());
            if (valueConvertible != nullptr) {
                return writeValueToJSONWriter(valueConvertible->toValue(), writer);
            } else {
                writer.writeString(value.toStringBox().toStringView());
            }
        } break;
        case ValueType::Function:
            writer.writeString(value.toStringBox().toStringView());
            break;
        case ValueType::Int:
        case ValueType::Long:
            writer.writeInt(value.toLong());
            break;
        case ValueType::BigInt:
            writer.writeString(value.getBigInt()->toString());
            break;
        case ValueType::Double:
            writer.writeDouble(value.toDouble());
            break;
        case ValueType::Bool:
            writer.writeBool(value.toBool());
            break;
        case ValueType::Map: {
            writer.writeBeginObject();

            auto isFirst = true;
            for (const auto& it : *value.getMap()) {
                if (!isFirst) {
                    writer.writeComma();
                }
                isFirst = false;
                writer.writeProperty(it.first.toStringView());
                writeValueToJSONWriter(it.second, writer);
            }

            writer.writeEndObject();
        } break;
        case ValueType::Array: {
            writer.writeArray(*value.getArray(), [&](const auto& item) { writeValueToJSONWriter(item, writer); });
        } break;
        case ValueType::TypedArray: {
            auto bytes = value.getTypedArray()->getBuffer();

            auto base64 = snap::utils::encoding::binaryToBase64(bytes.data(), bytes.size());

            writer.writeString(base64);
        } break;
    }
}

Value jsonValueToValue(const Json::Value& jsonValue) {
    switch (jsonValue.type()) {
        case Json::nullValue:
            return Value();
        case Json::intValue:
            [[fallthrough]];
        case Json::uintValue: {
            if (jsonValue.isInt()) {
                return Value(jsonValue.asInt());
            } else {
                return Value(static_cast<int64_t>(jsonValue.asInt64()));
            }
        }
        case Json::realValue:
            return Value(jsonValue.asDouble());
        case Json::stringValue: {
            const char* start;
            const char* end;
            jsonValue.getString(&start, &end);
            return Value(StringCache::getGlobal().makeString(start, end - start));
        }
        case Json::booleanValue:
            return Value(jsonValue.asBool());
        case Json::arrayValue: {
            auto size = jsonValue.size();
            auto out = ValueArray::make(static_cast<size_t>(size));

            size_t i = 0;
            for (const auto& value : jsonValue) {
                out->emplace(static_cast<size_t>(i++), jsonValueToValue(value));
            }

            return Value(out);
        }
        case Json::objectValue: {
            auto valueMap = makeShared<ValueMap>();

            auto it = jsonValue.begin();
            auto end = jsonValue.end();

            valueMap->reserve(end - it);

            while (it != end) {
                const char* end;
                const char* start = it.memberName(&end);
                auto key = StringCache::getGlobal().makeString(start, end - start);
                (*valueMap)[key] = jsonValueToValue(*it);

                it++;
            }

            return Value(valueMap);
        }
    }
}

Result<Value> correctJsonToValue(const std::string_view& str) {
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(str.data(), str.data() + str.size(), root)) {
        auto error = reader.getStructuredErrors()[0];
        return TextParser::makeParseError(str, error.message, static_cast<size_t>(error.offset_start));
    }

    return jsonValueToValue(root);
}

Result<Value> fastJsonToValue(const std::string_view& str) {
    JSONReader reader(str);
    auto output = readValueFromJSONReader(reader);
    if (reader.hasError()) {
        return reader.getError();
    }
    return output;
}

Result<Value> jsonToValue(const std::string_view& str) {
    return correctJsonToValue(str);
}

Result<Value> jsonToValue(const Byte* data, size_t size) {
    return jsonToValue(std::string_view(reinterpret_cast<const char*>(data), size));
}

Value readValueFromJSONReader(JSONReader& reader) {
    std::string str;
    double d;
    bool b;

    switch (reader.peekToken()) {
        case JSONReader::Token::Error:
            return Value();
        case JSONReader::Token::Object: {
            reader.parseBeginObject();
            auto valueMap = makeShared<ValueMap>();
            while (!reader.tryParseEndObject()) {
                if ((!valueMap->empty() && !reader.parseComma()) || reader.hasError()) {
                    return Value();
                }

                str.clear();
                if (!reader.parseString(str) || !reader.parseColon()) {
                    return Value();
                }

                auto value = readValueFromJSONReader(reader);
                if (reader.hasError()) {
                    return Value();
                }
                (*valueMap)[StringCache::getGlobal().makeString(str)] = std::move(value);
            }
            return Value(valueMap);
        }
        case JSONReader::Token::Array: {
            reader.parseBeginArray();

            ValueArrayBuilder arrayBuilder;

            while (!reader.tryParseEndArray()) {
                if ((!arrayBuilder.empty() && !reader.parseComma()) || reader.hasError()) {
                    return Value();
                }
                auto value = readValueFromJSONReader(reader);
                if (reader.hasError()) {
                    return Value();
                }
                arrayBuilder.append(std::move(value));
            }
            return Value(arrayBuilder.build());
        }
        case JSONReader::Token::String: {
            if (!reader.parseString(str)) {
                return Value();
            }
            return Value(str);
        }
        case JSONReader::Token::Number: {
            if (!reader.parseDouble(d)) {
                return Value();
            }
            return Value(d);
        }
        case JSONReader::Token::Boolean: {
            if (!reader.parseBool(b)) {
                return Value();
            }
            return Value(b);
        }
        case JSONReader::Token::Null:
            reader.parseNull();
            return Value();
    }
}

} // namespace Valdi
