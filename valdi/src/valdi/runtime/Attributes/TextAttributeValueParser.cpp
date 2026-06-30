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
    TextAttributeValueEntryTypeInlineImage,
    TextAttributeValueEntryTypePushAnimationTransform,
    TextAttributeValueEntryTypePushBackgroundColor,
    TextAttributeValueEntryTypePushBackgroundPadding,
    TextAttributeValueEntryTypePushBackgroundBorderRadius,
    TextAttributeValueEntryTypeInlineView,
};

STRING_CONST(objectReplacementCharacter, "\xEF\xBF\xBC")

static Error invalidTextAttributeValueError(const Value& value, std::string_view message) {
    return Error(STRING_FORMAT("Invalid TextAttributeValue: {} (value was '{}')", message, value.toString()));
}

using StylesStack = SmallVector<TextAttributeValueStyle, 16>;
using ColorStyleProperty = std::optional<Color> TextAttributeValueStyle::*;

TextAttributeValueStyle& pushStyle(StylesStack& stylesStack) {
    auto& newStyle = stylesStack.emplace_back();
    if (stylesStack.size() > 1) {
        newStyle = stylesStack[stylesStack.size() - 2];
        if (newStyle.background != nullptr) {
            newStyle.background = makeShared<TextBackgroundAttributeStyle>(*newStyle.background);
        }
    }

    return newStyle;
}

static TextBackgroundAttributeStyle& ensureBackground(TextAttributeValueStyle& style) {
    if (style.background == nullptr) {
        style.background = makeShared<TextBackgroundAttributeStyle>();
    }

    return *style.background;
}

static void logParseError(ILogger* logger, const std::optional<Error>& error) {
    if (logger == nullptr) {
        return;
    }
    VALDI_ERROR(*logger, "{}", error.value());
}

static std::optional<Color> parseColor(const ColorPalette* colorPalette, const Value& entryValue, Error& error) {
    if (colorPalette == nullptr) {
        return std::nullopt;
    }

    auto colorString = entryValue.toStringBox();
    AttributeParser parser(colorString.toStringView());
    auto color = parser.parseColor(*colorPalette);
    if (!color) {
        error = parser.getError();
        return std::nullopt;
    }

    return color.value();
}

static std::optional<Error> parseAndPushColorStyle(const ColorPalette* colorPalette,
                                                   const Value& value,
                                                   const Value& entryValue,
                                                   StylesStack& stylesStack,
                                                   ILogger* logger,
                                                   bool strict,
                                                   ColorStyleProperty colorStyleProperty) {
    Error parseError;
    auto color = parseColor(colorPalette, entryValue, parseError);
    if (!color) {
        if (colorPalette == nullptr) {
            pushStyle(stylesStack);
            return std::nullopt;
        }

        auto error = invalidTextAttributeValueError(value, parseError.toString());
        if (strict) {
            return error;
        }
        pushStyle(stylesStack);
        logParseError(logger, error);
        return std::nullopt;
    }

    pushStyle(stylesStack).*colorStyleProperty = color.value();
    return std::nullopt;
}

static std::optional<Error> parseBackgroundPaddingValue(const Value& map, std::string_view name, float& out) {
    auto value = map.getMapValue(name);
    if (value.isUndefined()) {
        return std::nullopt;
    }

    auto number = static_cast<float>(value.toDouble());
    if (number < 0) {
        return invalidTextAttributeValueError(map, "Background padding must be non-negative");
    }

    out = number;
    return std::nullopt;
}

static std::optional<Error> parseBackgroundPadding(const Value& value,
                                                   const Value& entryValue,
                                                   TextBackgroundPadding& padding) {
    if (entryValue.isNumber()) {
        auto number = static_cast<float>(entryValue.toDouble());
        if (number < 0) {
            return invalidTextAttributeValueError(value, "Background padding must be non-negative");
        }

        padding.left = number;
        padding.top = number;
        padding.right = number;
        padding.bottom = number;
        return std::nullopt;
    }

    if (!entryValue.isMap()) {
        return invalidTextAttributeValueError(value, "Background padding must be an object or number");
    }

    auto error = parseBackgroundPaddingValue(entryValue, "left", padding.left);
    if (!error) {
        error = parseBackgroundPaddingValue(entryValue, "top", padding.top);
    }
    if (!error) {
        error = parseBackgroundPaddingValue(entryValue, "right", padding.right);
    }
    if (!error) {
        error = parseBackgroundPaddingValue(entryValue, "bottom", padding.bottom);
    }

    return error;
}

template<typename Parser>
static std::optional<Error> parseAndPushBackgroundStyle(StylesStack& stylesStack,
                                                        ILogger* logger,
                                                        bool strict,
                                                        Parser parser) {
    auto& style = pushStyle(stylesStack);
    auto previousBackground = style.background;
    auto error = parser(ensureBackground(style));
    if (error) {
        if (strict) {
            return error.value();
        }
        style.background = previousBackground;
        logParseError(logger, error);
    }

    return std::nullopt;
}

static void appendTextAttributePart(TextAttributeValue::Parts& parts,
                                    const StylesStack& stylesStack,
                                    SmallVector<uint32_t, 8>& animationPartCounts,
                                    const StringBox& content,
                                    const std::optional<ImageAttachment>& imageAttachment,
                                    Ref<TextInlineAttachment> inlineViewAttachment) {
    auto& part = parts.emplace_back();
    part.content = content;

    if (!stylesStack.empty()) {
        part.style = stylesStack[stylesStack.size() - 1];
        if (part.style.animationTransform) {
            auto& animationTransform = part.style.animationTransform.value();
            if (animationTransform.groupIndex < animationPartCounts.size()) {
                animationTransform.partIndexInGroup = animationPartCounts[animationTransform.groupIndex]++;
            }
        }
    }

    if (imageAttachment) {
        part.style.imageAttachment = imageAttachment.value();
    }
    if (inlineViewAttachment != nullptr) {
        part.style.inlineViewAttachment = inlineViewAttachment;
    }
}

static Result<Ref<TextAttributeValue>> doParse(const ColorPalette* colorPalette,
                                               const Value& value,
                                               ILogger* logger,
                                               const Ref<TextInlineAttachment>* attachments,
                                               size_t attachmentsLength,
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
    SmallVector<uint32_t, 8> animationPartCounts;

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
                appendTextAttributePart(
                    parts, stylesStack, animationPartCounts, entryValue.toStringBox(), std::nullopt, nullptr);
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
                } else if (textDecorationString == "dashed-underline") {
                    textDecoration = TextDecoration::DashedUnderline;
                } else if (textDecorationString == "dotted-underline") {
                    textDecoration = TextDecoration::DottedUnderline;
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
                error = parseAndPushColorStyle(
                    colorPalette,
                    value,
                    entryValue,
                    stylesStack,
                    logger,
                    strict,
                    &TextAttributeValueStyle::color);
                if (error) {
                    return error.value();
                }
            } break;
            case TextAttributeValueEntryTypePushBackgroundColor: {
                if (colorPalette == nullptr) {
                    pushStyle(stylesStack);
                    break;
                }

                error = parseAndPushBackgroundStyle(
                    stylesStack,
                    logger,
                    strict,
                    [&](TextBackgroundAttributeStyle& backgroundStyle) -> std::optional<Error> {
                        Error parseError;
                        auto color = parseColor(colorPalette, entryValue, parseError);
                        if (!color) {
                            return invalidTextAttributeValueError(value, parseError.toString());
                        }
                        backgroundStyle.color = color.value();
                        return std::nullopt;
                    });
                if (error) {
                    return error.value();
                }
            } break;
            case TextAttributeValueEntryTypePushBackgroundPadding: {
                error = parseAndPushBackgroundStyle(
                    stylesStack,
                    logger,
                    strict,
                    [&](TextBackgroundAttributeStyle& backgroundStyle) -> std::optional<Error> {
                        TextBackgroundPadding padding;
                        auto error = parseBackgroundPadding(value, entryValue, padding);
                        if (error) {
                            return error.value();
                        }

                        backgroundStyle.padding = padding;
                        return std::nullopt;
                    });
                if (error) {
                    return error.value();
                }
            } break;
            case TextAttributeValueEntryTypePushBackgroundBorderRadius: {
                error = parseAndPushBackgroundStyle(
                    stylesStack,
                    logger,
                    strict,
                    [&](TextBackgroundAttributeStyle& backgroundStyle) -> std::optional<Error> {
                        std::optional<Dimension> dimension;
                        if (entryValue.isNumber()) {
                            dimension = Dimension(entryValue.toDouble(), Dimension::Unit::None);
                        } else if (entryValue.isString()) {
                            auto radiusString = entryValue.toStringBox();
                            AttributeParser parser(radiusString.toStringView());
                            dimension = parser.parseDimension();
                            if (dimension) {
                                parser.tryParseWhitespaces();
                                if (!parser.ensureIsAtEnd()) {
                                    dimension = std::nullopt;
                                }
                            }
                        }

                        if (!dimension) {
                            return invalidTextAttributeValueError(
                                value, "Background border radius must be a number or dimension");
                        }
                        if (dimension->value < 0) {
                            return invalidTextAttributeValueError(
                                value, "Background border radius must be non-negative");
                        }

                        backgroundStyle.borderRadius = dimension.value();
                        return std::nullopt;
                    });
                if (error) {
                    return error.value();
                }
            } break;
            case TextAttributeValueEntryTypePushOutlineColor: {
                error = parseAndPushColorStyle(
                    colorPalette,
                    value,
                    entryValue,
                    stylesStack,
                    logger,
                    strict,
                    &TextAttributeValueStyle::outlineColor);
                if (error) {
                    return error.value();
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
                error = parseAndPushColorStyle(
                    colorPalette,
                    value,
                    entryValue,
                    stylesStack,
                    logger,
                    strict,
                    &TextAttributeValueStyle::outerOutlineColor);
                if (error) {
                    return error.value();
                }
            } break;
            case TextAttributeValueEntryTypePushOuterOutlineWidth: {
                pushStyle(stylesStack).outerOutlineWidth = entryValue.toDouble();
            } break;
            case TextAttributeValueEntryTypePushAnimationTransform: {
                if (entryValue.isMap()) {
                    TextAnimationTransform animationTransform;
                    auto keyValue = entryValue.getMapValue("key");
                    if (!keyValue.isUndefined()) {
                        animationTransform.key = keyValue.toStringBox();
                    }
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
                    auto durationValue = entryValue.getMapValue("duration");
                    if (!durationValue.isUndefined()) {
                        animationTransform.duration = durationValue.toDouble();
                    }
                    auto timeOffsetBetweenPartsValue = entryValue.getMapValue("timeOffsetBetweenParts");
                    if (!timeOffsetBetweenPartsValue.isUndefined()) {
                        animationTransform.timeOffsetBetweenParts = timeOffsetBetweenPartsValue.toDouble();
                    }
                    auto partPatternValue = entryValue.getMapValue("partPattern");
                    if (!partPatternValue.isUndefined()) {
                        animationTransform.partPattern = partPatternValue.toStringBox();
                    }
                    animationTransform.groupIndex = static_cast<uint32_t>(animationPartCounts.size());
                    animationPartCounts.emplace_back(0);
                    pushStyle(stylesStack).animationTransform = animationTransform;
                } else {
                    pushStyle(stylesStack);
                }
            } break;
            case TextAttributeValueEntryTypeInlineImage: {
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
                            attachment.imageData = typedArray->getBuffer();
                        }
                    }

                    appendTextAttributePart(
                        parts, stylesStack, animationPartCounts, objectReplacementCharacter(), attachment, nullptr);
                }
            } break;
            case TextAttributeValueEntryTypeInlineView: {
                if (entryValue.isMap()) {
                    int32_t childIndex = -1;
                    auto childIndexValue = entryValue.getMapValue("childIndex");
                    if (!childIndexValue.isUndefined()) {
                        childIndex = childIndexValue.toInt();
                    }
                    if (childIndex < 0 || static_cast<size_t>(childIndex) >= attachmentsLength) {
                        error = {invalidTextAttributeValueError(value, "Invalid inline view child index")};
                        if (strict) {
                            return error.value();
                        }
                        logParseError(logger, error);
                        appendTextAttributePart(parts,
                                                stylesStack,
                                                animationPartCounts,
                                                objectReplacementCharacter(),
                                                std::nullopt,
                                                nullptr);
                        break;
                    }
                    const auto& attachment = attachments[static_cast<size_t>(childIndex)];

                    auto verticalAlignmentValue = entryValue.getMapValue("verticalAlignment");
                    if (!verticalAlignmentValue.isUndefined()) {
                        auto verticalAlignment = verticalAlignmentValue.toInt();
                        if (verticalAlignment == static_cast<int32_t>(InlineViewVerticalAlignment::Top)) {
                            attachment->setVerticalAlignment(InlineViewVerticalAlignment::Top);
                        } else if (verticalAlignment == static_cast<int32_t>(InlineViewVerticalAlignment::Bottom)) {
                            attachment->setVerticalAlignment(InlineViewVerticalAlignment::Bottom);
                        } else if (verticalAlignment == static_cast<int32_t>(InlineViewVerticalAlignment::Baseline)) {
                            attachment->setVerticalAlignment(InlineViewVerticalAlignment::Baseline);
                        } else {
                            attachment->setVerticalAlignment(InlineViewVerticalAlignment::Center);
                        }
                    } else {
                        attachment->setVerticalAlignment(InlineViewVerticalAlignment::Center);
                    }

                    appendTextAttributePart(parts,
                                            stylesStack,
                                            animationPartCounts,
                                            objectReplacementCharacter(),
                                            std::nullopt,
                                            attachment);
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
                                              const Ref<TextInlineAttachment>* attachments,
                                              size_t attachmentsLength,
                                              bool strict) {
    auto parsed = doParse(&colorPalette, value, &logger, attachments, attachmentsLength, strict);
    if (!parsed) {
        return parsed.moveError();
    }

    return Value(parsed.value());
}

StringBox TextAttributeValueParser::toString(const Value& value) {
    auto parsed = doParse(nullptr, value, nullptr, nullptr, 0, false);
    if (!parsed) {
        return StringBox();
    }

    return StringCache::getGlobal().makeString(parsed.value()->toString());
}

} // namespace Valdi
