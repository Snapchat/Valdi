//
//  TextAttributeValue.cpp
//  valdi_core-ios
//
//  Created by Simon Corsin on 12/20/22.
//

#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"

namespace Valdi {

TextAttributeValue::TextAttributeValue(TextAttributeValueBase<TextAttributeValueStyle>::Parts parts)
    : TextAttributeValueBase<TextAttributeValueStyle>(std::move(parts)) {
    const auto length = getPartsSize();
    for (size_t i = 0; i < length; i++) {
        if (getStyleAtIndex(i).animationTransform.has_value()) {
            _animationTransformsSize++;
        }
    }
}
TextAttributeValue::~TextAttributeValue() = default;

std::string TextAttributeValue::toString() const {
    std::string out;

    size_t length = getPartsSize();
    for (size_t i = 0; i < length; i++) {
        out += getContentAtIndex(i).toStringView();
    }

    return out;
}

size_t TextAttributeValue::getAnimationTransformsSize() const {
    return _animationTransformsSize;
}

VALDI_CLASS_IMPL(TextAttributeValue)

} // namespace Valdi
