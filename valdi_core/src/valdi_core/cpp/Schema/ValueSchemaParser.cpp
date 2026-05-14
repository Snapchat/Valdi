//
//  ValueSchemaParser.cpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 1/19/23.
//

#include "valdi_core/cpp/Schema/ValueSchemaParser.hpp"
#include "ValueSchemaParser.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/TextParser.hpp"

namespace Valdi {

template<typename T>
using ParseListVector = SmallVector<T, 20>;

template<typename T, typename F>
static bool readList(TextParser& parser, char characterStart, char characterEnd, F&& itemParser) {
    if (!parser.parse(characterStart)) {
        return false;
    }

    bool needsComma = false;

    for (;;) {
        if (!parser.ensureNotAtEnd()) {
            return false;
        }
        if (parser.tryParse(characterEnd)) {
            break;
        }
        if (needsComma) {
            if (!parser.parse(',')) {
                break;
            }
            parser.tryParseWhitespaces();
        } else {
            needsComma = true;
        }

        auto item = itemParser(parser);
        if (!item) {
            return false;
        }
    }

    return true;
}

template<typename T, typename F>
static std::optional<ParseListVector<T>> parseList(TextParser& parser,
                                                   char characterStart,
                                                   char characterEnd,
                                                   F&& itemParser) {
    ParseListVector<T> vector;

    auto success = readList<F>(parser, characterStart, characterEnd, [&](TextParser& parser) {
        auto item = itemParser(parser);
        if (!item) {
            return false;
        }
        vector.emplace_back(std::move(item.value()));

        return true;
    });

    return success ? std::make_optional(vector) : std::nullopt;
}

static std::optional<StringBox> parseString(TextParser& parser) {
    if (!parser.parse('\'')) {
        return std::nullopt;
    }

    auto str = parser.readUntilCharacterAndParse('\'');

    if (!str) {
        return std::nullopt;
    }

    return StringCache::getGlobal().makeString(str.value());
}

static std::optional<ValueSchemaTypeReference> parseTypeReference(TextParser& parser) {
    auto typeHint = ValueSchemaTypeReferenceTypeHintUnknown;
    if (parser.tryParse('<')) {
        if (parser.tryParse(kValueSchemaTypeReferenceTypeHintUnknownStr[0])) {
            typeHint = ValueSchemaTypeReferenceTypeHintUnknown;
        } else if (parser.tryParse(kValueSchemaTypeReferenceTypeHintObjectStr[0])) {
            typeHint = ValueSchemaTypeReferenceTypeHintObject;
        } else if (parser.tryParse(kValueSchemaTypeReferenceTypeHintEnumStr[0])) {
            typeHint = ValueSchemaTypeReferenceTypeHintEnum;
        } else if (parser.tryParse(kValueSchemaTypeReferenceTypeHintConvertedStr[0])) {
            typeHint = ValueSchemaTypeReferenceTypeHintConverted;
        } else {
            parser.setErrorAtCurrentPosition("Invalid type reference type hint");
            return std::nullopt;
        }

        if (!parser.parse('>')) {
            return std::nullopt;
        }
    }

    if (!parser.parse(':')) {
        return std::nullopt;
    }

    if (parser.peek('\'')) {
        auto strResult = parseString(parser);
        if (!strResult) {
            return std::nullopt;
        }
        return ValueSchemaTypeReference::named(typeHint, strResult.value());
    } else {
        auto position = parser.parseInt();
        if (!position) {
            return std::nullopt;
        }
        return ValueSchemaTypeReference::positional(static_cast<ValueSchemaTypeReference::Position>(position.value()));
    }
}

static std::optional<ValueSchema> parseUntypedSchema(TextParser& /*parser*/) {
    return ValueSchema::untyped();
}

static std::optional<ValueSchema> parseVoidSchema(TextParser& /*parser*/) {
    return ValueSchema::voidType();
}

static std::optional<ValueSchema> parseIntSchema(TextParser& /*parser*/) {
    return ValueSchema::integer();
}

static std::optional<ValueSchema> parseLongSchema(TextParser& /*parser*/) {
    return ValueSchema::longInteger();
}

static std::optional<ValueSchema> parseDoubleSchema(TextParser& /*parser*/) {
    return ValueSchema::doublePrecision();
}

static std::optional<ValueSchema> parseBoolSchema(TextParser& /*parser*/) {
    return ValueSchema::boolean();
}

static std::optional<ValueSchema> parseStringSchema(TextParser& /*parser*/) {
    return ValueSchema::string();
}

static std::optional<ValueSchema> parseValueTypedArraySchema(TextParser& /*parser*/) {
    return ValueSchema::valueTypedArray();
}

static std::optional<ValueSchema> parseTypeReferenceSchema(TextParser& parser) {
    auto typeReference = parseTypeReference(parser);
    if (!typeReference) {
        return std::nullopt;
    }

    return ValueSchema::typeReference(typeReference.value());
}

static std::optional<ValueSchema> parseGenericTypeReferenceSchema(TextParser& parser) {
    auto typeReference = parseTypeReference(parser);
    if (!typeReference) {
        return std::nullopt;
    }

    auto typeArguments =
        parseList<ValueSchema>(parser, '<', '>', [](TextParser& parser) { return ValueSchemaParser::parse(parser); });

    if (!typeArguments) {
        return std::nullopt;
    }

    return ValueSchema::genericTypeReference(
        typeReference.value(), typeArguments.value().data(), typeArguments.value().size());
}

static std::optional<ValueSchema> doParseClassSchema(TextParser& parser, bool includeProperties) {
    auto isInterface = parser.tryParse(kStringTypeClassModifierInterface[0]);

    parser.tryParseWhitespaces();

    auto classNameResult = parseString(parser);
    if (!classNameResult) {
        return std::nullopt;
    }
    parser.tryParseWhitespaces();

    if (!includeProperties) {
        return ValueSchema::cls(classNameResult.value(), isInterface, {});
    }

    auto properties =
        parseList<ClassPropertySchema>(parser, '{', '}', [](TextParser& parser) -> std::optional<ClassPropertySchema> {
            auto propertyName = parseString(parser);
            if (!propertyName) {
                return std::nullopt;
            }

            if (!parser.parse(':')) {
                return std::nullopt;
            }

            parser.tryParseWhitespaces();

            auto propertyType = ValueSchemaParser::parse(parser);
            if (!propertyType) {
                return std::nullopt;
            }

            return ClassPropertySchema(propertyName.value(), propertyType.value());
        });

    if (!properties) {
        return std::nullopt;
    }

    return ValueSchema::cls(classNameResult.value(), isInterface, properties.value().data(), properties.value().size());
}

static std::optional<ValueSchema> parseClassSchema(TextParser& parser) {
    return doParseClassSchema(parser, true);
}

static std::optional<ValueSchema> parseEnumSchema(TextParser& parser) {
    if (!parser.parse('<')) {
        return std::nullopt;
    }
    auto enumCaseType = ValueSchemaParser::parse(parser);
    if (!enumCaseType) {
        return std::nullopt;
    }

    if (!parser.parse('>')) {
        return std::nullopt;
    }

    parser.tryParseWhitespaces();

    auto enumNameResult = parseString(parser);
    if (!enumNameResult) {
        return std::nullopt;
    }

    parser.tryParseWhitespaces();

    auto enumCases =
        parseList<EnumCaseSchema>(parser, '{', '}', [](TextParser& parser) -> std::optional<EnumCaseSchema> {
            auto caseName = parseString(parser);
            if (!caseName) {
                return std::nullopt;
            }

            if (!parser.parse(':')) {
                return std::nullopt;
            }

            parser.tryParseWhitespaces();

            Value value;
            if (parser.peek('\'')) {
                auto valueString = parseString(parser);
                if (!valueString) {
                    return std::nullopt;
                }
                value = Value(valueString.value());
            } else {
                auto valueInt = parser.parseInt();
                if (!valueInt) {
                    return std::nullopt;
                }
                value = Value(static_cast<int32_t>(valueInt.value()));
            }

            return EnumCaseSchema(caseName.value(), value);
        });

    if (!enumCases) {
        return std::nullopt;
    }

    return ValueSchema::enumeration(
        enumNameResult.value(), enumCaseType.value(), enumCases.value().data(), enumCases.value().size());
}

ValueFunctionSchemaAttributes parseFunctionAttributes(TextParser& parser) {
    // Support for legacy function modifiers syntax
    bool isMethod = parser.tryParse("*");
    bool isSingleCall = parser.tryParse("!");
    bool isWorkerThread = false;
    bool allowSyncCall = true; // Default: allow sync (omit `b` / bansync in schema)

    if (parser.peek('|')) {
        readList<ValueSchema>(parser, '|', '|', [&](TextParser& parser) -> bool {
            if (parser.tryParse(kStringTypeFunctionAttributeMethod[0])) {
                isMethod = true;
            } else if (parser.tryParse(kStringTypeFunctionAttributeSingleCall[0])) {
                isSingleCall = true;
            } else if (parser.tryParse(kStringTypeFunctionAttributeWorkerThread[0])) {
                isWorkerThread = true;
            } else if (parser.tryParse(kStringTypeFunctionAttributeBanSync[0])) {
                allowSyncCall = false; // `b` = bansync (async_strict, no @AllowSyncCall)
            }
            return true;
        });
    }

    return ValueFunctionSchemaAttributes(isMethod, isSingleCall, isWorkerThread, allowSyncCall);
}

static std::optional<ValueSchema> parseFunctionSchema(TextParser& parser) {
    auto functionAttributes = parseFunctionAttributes(parser);
    auto arguments = parseList<ValueSchema>(parser, '(', ')', [](TextParser& parser) -> std::optional<ValueSchema> {
        return ValueSchemaParser::parse(parser);
    });
    if (!arguments) {
        return std::nullopt;
    }

    ValueSchema returnValue = ValueSchema::voidType();

    if (parser.tryParse(':')) {
        parser.tryParseWhitespaces();
        auto parsedReturnValue = ValueSchemaParser::parse(parser);
        if (!parsedReturnValue) {
            return std::nullopt;
        }

        returnValue = std::move(parsedReturnValue.value());
    }

    return ValueSchema::function(functionAttributes, returnValue, arguments.value().data(), arguments.value().size());
}

static std::optional<ValueSchema> parseArraySchema(TextParser& parser) {
    if (!parser.parse('<')) {
        return std::nullopt;
    }
    auto itemType = ValueSchemaParser::parse(parser);
    if (!itemType) {
        return std::nullopt;
    }
    if (!parser.parse('>')) {
        return std::nullopt;
    }

    return ValueSchema::array(std::move(itemType.value()));
}

static std::optional<ValueSchema> parseMapSchema(TextParser& parser) {
    auto keyValues = parseList<ValueSchema>(parser, '<', '>', [](TextParser& parser) -> std::optional<ValueSchema> {
        return ValueSchemaParser::parse(parser);
    });
    if (!keyValues) {
        return std::nullopt;
    }
    if (keyValues.value().size() != 2) {
        parser.setErrorAtCurrentPosition("map definition should have two entries");
        return std::nullopt;
    }

    return ValueSchema::map(std::move(keyValues.value()[0]), std::move(keyValues.value()[1]));
}

static std::optional<ValueSchema> parseES6MapSchema(TextParser& parser) {
    auto keyValues = parseList<ValueSchema>(parser, '<', '>', [](TextParser& parser) -> std::optional<ValueSchema> {
        return ValueSchemaParser::parse(parser);
    });
    if (!keyValues) {
        return std::nullopt;
    }

    if (keyValues.value().size() != 2) {
        parser.setErrorAtCurrentPosition("map definition should have two entries");
        return std::nullopt;
    }

    return ValueSchema::es6map(std::move(keyValues.value()[0]), std::move(keyValues.value()[1]));
}

static std::optional<ValueSchema> parseES6SetSchema(TextParser& parser) {
    auto keyValues = parseList<ValueSchema>(parser, '<', '>', [](TextParser& parser) -> std::optional<ValueSchema> {
        return ValueSchemaParser::parse(parser);
    });
    if (!keyValues) {
        return std::nullopt;
    }

    if (keyValues.value().size() != 1) {
        parser.setErrorAtCurrentPosition("set definition should have one entry");
        return std::nullopt;
    }

    return ValueSchema::es6set(std::move(keyValues.value()[0]));
}

static std::optional<ValueSchema> parsePromiseSchema(TextParser& parser) {
    if (!parser.parse('<')) {
        return std::nullopt;
    }
    auto itemType = ValueSchemaParser::parse(parser);
    if (!itemType) {
        return std::nullopt;
    }
    if (!parser.parse('>')) {
        return std::nullopt;
    }

    return ValueSchema::promise(std::move(itemType.value()));
}

static std::optional<ValueSchema> parseRefSchema(TextParser& parser) {
    parser.setErrorAtCurrentPosition("Schema references cannot be parsed");
    return std::nullopt;
}

Result<ValueSchema> ValueSchemaParser::parse(std::string_view str) {
    TextParser parser(str);
    auto result = parse(parser);
    if (!result) {
        return parser.getError();
    }
    if (!parser.ensureIsAtEnd()) {
        return parser.getError();
    }
    return result.value();
}

using ValueSchemaTypeParserFn = std::optional<ValueSchema> (*)(TextParser& parser);

constexpr std::array<std::pair<std::string_view, ValueSchemaTypeParserFn>, 19> kTypeParsers = {{
    {kStringTypeUntyped, &parseUntypedSchema},
    {kStringTypeVoid, &parseVoidSchema},
    {kStringTypeInt, &parseIntSchema},
    {kStringTypeLong, &parseLongSchema},
    {kStringTypeDouble, &parseDoubleSchema},
    {kStringTypeBool, &parseBoolSchema},
    {kStringTypeString, &parseStringSchema},
    {kStringTypeValueTypedArray, &parseValueTypedArraySchema},
    {kStringTypeTypeReference, &parseTypeReferenceSchema},
    {kStringTypeGenericTypeReference, &parseGenericTypeReferenceSchema},
    {kStringTypeClass, &parseClassSchema},
    {kStringTypeEnum, &parseEnumSchema},
    {kStringTypeFunction, &parseFunctionSchema},
    {kStringTypeArray, &parseArraySchema},
    {kStringTypeMap, &parseMapSchema},
    {kStringTypeES6Map, &parseES6MapSchema},
    {kStringTypeES6Set, &parseES6SetSchema},
    {kStringTypePromise, &parsePromiseSchema},
    {kStringTypeSchemaRef, &parseRefSchema},
}};

std::optional<ValueSchema> ValueSchemaParser::parse(TextParser& parser) {
    for (const auto& typeParser : kTypeParsers) {
        if (parser.tryParse(typeParser.first[0])) {
            auto isBoxed = parser.tryParse('@');
            auto isOptional = parser.tryParse('?');
            auto schema = typeParser.second(parser);
            if (!schema) {
                parser.prependError(fmt::format("Failed to parse schema of type '{}'", typeParser.first));
                return std::nullopt;
            }

            if (isBoxed) {
                schema.value() = ValueSchema::boxed(std::move(schema.value()));
            }

            if (isOptional) {
                return ValueSchema::optional(std::move(schema.value()));
            } else {
                return schema.value();
            }
        }
    }

    parser.setErrorAtCurrentPosition("Unrecognized token");

    return std::nullopt;
}

std::optional<ValueSchema> ValueSchemaParser::parseClassSchema(TextParser& parser, bool includeProperties) {
    if (!parser.tryParse(kStringTypeClass[0])) {
        return std::nullopt;
    }

    return doParseClassSchema(parser, true);
}

} // namespace Valdi
