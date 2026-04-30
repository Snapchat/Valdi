//
//  TextAttributeValueParser.cpp
//  valdi
//
//  Created by Simon Corsin on 12/20/22.
//

#include "valdi/runtime/Attributes/TextAttributeValueParser.hpp"
#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

namespace Valdi {

enum TextAttributeValueEntryType : int32_t {
    TextAttributeValueEntryTypeContent = 1,
    TextAttributeValueEntryTypePop,
    TextAttributeValueEntryTypePushFont,
    TextAttributeValueEntryTypePushTextDecoration,
    TextAttributeValueEntryTypePushColor,
    TextAttributeValueEntryTypePushOnTap,
    TextAttributeValueEntryTypePushOnLayout,
    TextAttributeValueEntryTypePushOutlineColor,
    TextAttributeValueEntryTypePushOutlineWidth,
    TextAttributeValueEntryTypePushOuterOutlineColor,
    TextAttributeValueEntryTypePushOuterOutlineWidth,
    TextAttributeValueEntryTypePushInlineImage,
    TextAttributeValueEntryTypePushAnimationTransform,
};

static Error invalidTextAttributeValueError(const Value& value, std::string_view message) {
    return Error(STRING_FORMAT("Invalid TextAttributeValue: {} (value was '{}')", message, value.toString()));
}

using StylesStack = SmallVector<TextAttributeValueStyle, 16>;

TextAttributeValueStyle& pushStyle(StylesStack& stylesStack) {
    auto& newStyle = stylesStack.emplace_back();
    if (stylesStack.size() > 1) {
        newStyle = stylesStack[stylesStack.size() - 2];
    }

    return newStyle;
}

static void logParseError(ILogger* logger, const std::optional<Error>& error) {
    if (logger == nullptr) {
        return;
    }
    VALDI_ERROR(*logger, "{}", error.value());
}

static Result<Ref<TextAttributeValue>> doParse(const ColorPalette* colorPalette,
                                               const Value& value,
                                               ILogger* logger,
                                               bool strict) {
    const auto* components = value.getArray();
    if (components == nullptr) {
        return invalidTextAttributeValueError(value, "Expecting array");
    }

    if (components->empty()) {
        return invalidTextAttributeValueError(value, "Empty components");
    }

    std::optional<Error> error;
    TextAttributeValue::Parts parts;
    StylesStack stylesStack;

    size_t index = 0;
    auto size = components->size();

    while (index < size) {
        auto type = static_cast<TextAttributeValueEntryType>((*components)[index].toInt());
        auto entrySize = (type == TextAttributeValueEntryTypePop) ? 1 : 2;
        if (index + entrySize > size) {
            error = {invalidTextAttributeValueError(value, "Missing entries")};
            if (strict) {
                return error.value();
            } else {
                logParseError(logger, error);
                break;
            }
        }
        const auto& entryValue = (*components)[index + entrySize - 1];

        switch (type) {
            case TextAttributeValueEntryTypeContent: {
                auto content = entryValue.toStringBox();
                auto& part = parts.emplace_back();
                part.content = entryValue.toStringBox();

                if (!stylesStack.empty()) {
                    part.style = stylesStack[stylesStack.size() - 1];
                }
            } break;
            case TextAttributeValueEntryTypePop:
                if (stylesStack.empty()) {
                    error = {invalidTextAttributeValueError(value, "Unbalanced styles stack")};
                    if (strict) {
                        return error.value();
                    } else {
                        logParseError(logger, error);
                    }
                } else {
                    stylesStack.pop_back();
                }
                break;
            case TextAttributeValueEntryTypePushFont:
                pushStyle(stylesStack).font = entryValue.toStringBox();
                break;
            case TextAttributeValueEntryTypePushTextDecoration: {
                auto textDecorationString = entryValue.toStringBox();
                auto textDecoration = TextDecoration::Unset;
                if (textDecorationString == "none") {
                    textDecoration = TextDecoration::None;
                } else if (textDecorationString == "underline") {
                    textDecoration = TextDecoration::Underline;
                } else if (textDecorationString == "strikethrough") {
                    textDecoration = TextDecoration::Strikethrough;
                } else {
                    error = {invalidTextAttributeValueError(value, "Invalid text decoration")};

                    if (strict) {
                        return error.value();
                    } else {
                        logParseError(logger, error);
                    }
                }
                pushStyle(stylesStack).textDecoration = textDecoration;
            } break;
            case TextAttributeValueEntryTypePushColor: {
                if (colorPalette != nullptr) {
                    auto colorString = entryValue.toStringBox();
                    AttributeParser parser(colorString.toStringView());
                    auto color = parser.parseColor(*colorPalette);
                    if (color) {
                        pushStyle(stylesStack).color = color.value();
                    } else {
                        error = {invalidTextAttributeValueError(value, parser.getError().toString())};
                        if (strict) {
                            return error.value();
                        } else {
                            // push empty style to keep balance
                            pushStyle(stylesStack);
                            logParseError(logger, error);
                        }
                    }
                } else {
                    // push empty style to keep balance
                    pushStyle(stylesStack);
                }
            } break;
            case TextAttributeValueEntryTypePushOutlineColor: {
                if (colorPalette != nullptr) {
                    auto colorString = entryValue.toStringBox();
                    AttributeParser parser(colorString.toStringView());
                    auto outlineColor = parser.parseColor(*colorPalette);
                    if (outlineColor) {
                        pushStyle(stylesStack).outlineColor = outlineColor.value();
                    } else {
                        error = {invalidTextAttributeValueError(value, parser.getError().toString())};
                        if (strict) {
                            return error.value();
                        } else {
                            // push empty style to keep balance
                            pushStyle(stylesStack);
                            logParseError(logger, error);
                        }
                    }
                } else {
                    // push empty style to keep balance
                    pushStyle(stylesStack);
                }
            } break;
            case TextAttributeValueEntryTypePushOnTap: {
                pushStyle(stylesStack).onTap = entryValue.getFunctionRef();
            } break;
            case TextAttributeValueEntryTypePushOnLayout: {
                pushStyle(stylesStack).onLayout = entryValue.getFunctionRef();
            } break;
            case TextAttributeValueEntryTypePushOutlineWidth: {
                pushStyle(stylesStack).outlineWidth = entryValue.toDouble();
            } break;
            case TextAttributeValueEntryTypePushOuterOutlineColor: {
                if (colorPalette != nullptr) {
                    auto colorString = entryValue.toStringBox();
                    AttributeParser parser(colorString.toStringView());
                    auto outerOutlineColor = parser.parseColor(*colorPalette);
                    if (outerOutlineColor) {
                        pushStyle(stylesStack).outerOutlineColor = outerOutlineColor.value();
                    } else {
                        error = {invalidTextAttributeValueError(value, parser.getError().toString())};
                        if (strict) {
                            return error.value();
                        } else {
                            // push empty style to keep balance
                            pushStyle(stylesStack);
                            logParseError(logger, error);
                        }
                    }
                } else {
                    // push empty style to keep balance
                    pushStyle(stylesStack);
                }
            } break;
            case TextAttributeValueEntryTypePushOuterOutlineWidth: {
                pushStyle(stylesStack).outerOutlineWidth = entryValue.toDouble();
            } break;
            case TextAttributeValueEntryTypePushAnimationTransform: {
                if (entryValue.isMap()) {
                    TextAnimationTransform animationTransform;
                    auto translationYValue = entryValue.getMapValue("translationY");
                    if (!translationYValue.isUndefined()) {
                        animationTransform.translationY = static_cast<float>(translationYValue.toDouble());
                    }
                    auto scaleValue = entryValue.getMapValue("scale");
                    if (!scaleValue.isUndefined()) {
                        animationTransform.scale = static_cast<float>(scaleValue.toDouble());
                    }
                    auto opacityValue = entryValue.getMapValue("opacity");
                    if (!opacityValue.isUndefined()) {
                        animationTransform.opacity = static_cast<float>(opacityValue.toDouble());
                    }
                    pushStyle(stylesStack).animationTransform = animationTransform;
                } else {
                    pushStyle(stylesStack);
                }
            } break;
            case TextAttributeValueEntryTypePushInlineImage: {
                if (entryValue.isMap()) {
                    ImageAttachment attachment;
                    auto attachmentIdValue = entryValue.getMapValue("attachmentId");
                    if (!attachmentIdValue.isUndefined()) {
                        attachment.attachmentId = attachmentIdValue.toStringBox();
                    }
                    auto widthValue = entryValue.getMapValue("width");
                    if (!widthValue.isUndefined()) {
                        attachment.width = static_cast<float>(widthValue.toDouble());
                    }
                    auto heightValue = entryValue.getMapValue("height");
                    if (!heightValue.isUndefined()) {
                        attachment.height = static_cast<float>(heightValue.toDouble());
                    }
                    auto imageDataValue = entryValue.getMapValue("imageData");
                    if (!imageDataValue.isUndefined()) {
                        const auto* typedArray = imageDataValue.getTypedArray();
                        if (typedArray != nullptr) {
                            const auto& buffer = typedArray->getBuffer();
                            attachment.imageData.assign(buffer.begin(), buffer.end());
                        }
                    }

                    pushStyle(stylesStack).imageAttachment = std::move(attachment);
                } else {
                    pushStyle(stylesStack);
                }
            } break;
            default:
                return invalidTextAttributeValueError(value, "Invalid entry type");
        }

        index += entrySize;
    }

    if (!stylesStack.empty()) {
        error = {invalidTextAttributeValueError(value, "Unbalanced styles stack")};
        if (strict) {
            return error.value();
        } else {
            logParseError(logger, error);
        }
    }

    return makeShared<TextAttributeValue>(std::move(parts));
}

Result<Value> TextAttributeValueParser::parse(const ColorPalette& colorPalette,
                                              const Value& value,
                                              ILogger& logger,
                                              bool strict) {
    auto parsed = doParse(&colorPalette, value, &logger, strict);
    if (!parsed) {
        return parsed.moveError();
    }

    return Value(parsed.value());
}

StringBox TextAttributeValueParser::toString(const Value& value) {
    auto parsed = doParse(nullptr, value, nullptr, false);
    if (!parsed) {
        return StringBox();
    }

    return StringCache::getGlobal().makeString(parsed.value()->toString());
}

} // namespace Valdi
