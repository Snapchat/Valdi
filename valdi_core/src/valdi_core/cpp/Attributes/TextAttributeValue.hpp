//
//  TextAttributeValue.hpp
//  valdi_core-ios
//
//  Created by Simon Corsin on 12/20/22.
//

#pragma once

#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include <vector>

namespace Valdi {

template<typename StyleType>
struct TextAttributeValuePart {
    StringBox content;
    StyleType style;
};

template<typename StyleType>
using TextAttributeValuePartVector = SmallVector<TextAttributeValuePart<StyleType>, 4>;

template<typename StyleType>
class TextAttributeValueBase {
public:
    using Parts = TextAttributeValuePartVector<StyleType>;

    TextAttributeValueBase(Parts parts) : _parts(std::move(parts)) {}
    ~TextAttributeValueBase() = default;

    size_t getPartsSize() const {
        return _parts.size();
    }

    const StringBox& getContentAtIndex(size_t index) const {
        return _parts[index].content;
    }

    const StyleType& getStyleAtIndex(size_t index) const {
        return _parts[index].style;
    }

private:
    Parts _parts;
};

enum class TextDecoration {
    Unset,
    None,
    Strikethrough,
    Underline,
};

/**
 * Represents an inline image attachment that can be embedded within attributed text.
 * Used for rendering images (like LaTeX equations) inline with text using native text layout.
 */
struct ImageAttachment {
    /** Unique identifier for the image attachment */
    StringBox attachmentId;
    /** Width of the image in logical pixels */
    float width = 0;
    /** Height of the image in logical pixels */
    float height = 0;
    /** PNG image data. Empty if placeholder. */
    std::vector<uint8_t> imageData;
};

struct TextAnimationTransform {
    float translationY = 0;
    float scale = 1;
    float opacity = 1;
};

struct TextAttributeValueStyle {
    std::optional<StringBox> font;
    TextDecoration textDecoration = TextDecoration::Unset;
    std::optional<Color> color;
    Ref<ValueFunction> onTap;
    Ref<ValueFunction> onLayout;
    std::optional<Color> outlineColor;
    std::optional<float> outlineWidth;
    std::optional<Color> outerOutlineColor;
    std::optional<float> outerOutlineWidth;
    std::optional<ImageAttachment> imageAttachment;
    std::optional<TextAnimationTransform> animationTransform;
};

/**
 TextAttributeValue is a deserialized representation of a text attribute containing
 an attributed text. It contains a list of parts where each part has a string content
 and an associated style.
 */
class TextAttributeValue : public ValdiObject, public TextAttributeValueBase<TextAttributeValueStyle> {
public:
    TextAttributeValue(TextAttributeValueBase<TextAttributeValueStyle>::Parts parts);
    ~TextAttributeValue() override;

    /**
     Return the entire content of the TextAttributeValue as a string, ignoring the styles.
     */
    std::string toString() const;

    VALDI_CLASS_HEADER(TextAttributeValue)
};

} // namespace Valdi
