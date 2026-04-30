//
//  ValueSchemaParser.hpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 1/19/23.
//

#pragma once

#include "valdi_core/cpp/Schema/ValueSchema.hpp"

namespace Valdi {

constexpr std::string_view kStringTypeUntyped = "untyped";
constexpr std::string_view kStringTypeVoid = "void";
constexpr std::string_view kStringTypeInt = "int";
constexpr std::string_view kStringTypeLong = "long";
constexpr std::string_view kStringTypeDouble = "double";
constexpr std::string_view kStringTypeBool = "bool";
constexpr std::string_view kStringTypeString = "string";
constexpr std::string_view kStringTypeValueTypedArray = "typedarray";
constexpr std::string_view kStringTypeTypeReference = "ref";
constexpr std::string_view kStringTypeGenericTypeReference = "genref";
constexpr std::string_view kStringTypeClass = "class";
constexpr std::string_view kStringTypeEnum = "enum";
constexpr std::string_view kStringTypeFunction = "func";
constexpr std::string_view kStringTypeArray = "array";
constexpr std::string_view kStringTypeMap = "map";
constexpr std::string_view kStringTypeES6Map = "%es6map";
constexpr std::string_view kStringTypeES6Set = "@es6set";
constexpr std::string_view kStringTypeOutcome = "~outcome";
constexpr std::string_view kStringTypeDate = "^date";
constexpr std::string_view kStringTypeProto = "#proto";
constexpr std::string_view kStringTypePromise = "promise";
constexpr std::string_view kStringTypeSchemaRef = "link";
constexpr std::string_view kStringTypeClassModifierInterface = "+";
constexpr std::string_view kStringTypeFunctionAttributeMethod = "method";
constexpr std::string_view kStringTypeFunctionAttributeSingleCall = "singlecall";
constexpr std::string_view kStringTypeFunctionAttributeWorkerThread = "workerthread";
/** Compact token is first char `b` (same pattern as method/singlecall/workerthread). Human-readable prints full word.
 */
constexpr std::string_view kStringTypeFunctionAttributeBanSync = "bansync";

class TextParser;

class ValueSchemaParser {
public:
    static Result<ValueSchema> parse(std::string_view str);
    static std::optional<ValueSchema> parse(TextParser& parser);
    static std::optional<ValueSchema> parseClassSchema(TextParser& parser, bool includeProperties);
};

} // namespace Valdi
