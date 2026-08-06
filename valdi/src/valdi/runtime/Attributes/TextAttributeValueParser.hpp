//
//  TextAttributeValueParser.hpp
//  valdi
//
//  Created by Simon Corsin on 12/20/22.
//

#pragma once

#include "valdi_core/cpp/Attributes/TextInlineAttachment.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <cstddef>

namespace Valdi {

class ColorPalette;
class ILogger;

class TextAttributeValueParser {
public:
    /**
     Parse the TextAttributeValue from a ValueArray representation into
     an TextAttributeValue object wrapped in a Value.
     If "strict" is true, any parse error will cause the parse to fail.
     If "strict" is false, parse errors that are recoverable will be logged
     without causing the whole parsing to fail.
     */
    static Result<Value> parse(const ColorPalette& colorPalette,
                               const Value& value,
                               ILogger& logger,
                               const Ref<TextInlineAttachment>* attachments,
                               size_t attachmentsLength,
                               bool strict);

    /**
     Parse the TextAttributeValue from a ValueArray representation and output a string
     representing all the content within it without the styles.
     */
    static StringBox toString(const Value& value);
};

} // namespace Valdi
