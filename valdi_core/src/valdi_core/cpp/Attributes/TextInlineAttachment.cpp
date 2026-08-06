//
//  TextInlineAttachment.cpp
//  valdi_core
//

#include "valdi_core/cpp/Attributes/TextInlineAttachment.hpp"

namespace Valdi {

TextInlineAttachment::TextInlineAttachment(size_t childIndex) : _childIndex(childIndex) {}

TextInlineAttachment::~TextInlineAttachment() = default;

size_t TextInlineAttachment::getChildIndex() const {
    return _childIndex;
}

InlineViewVerticalAlignment TextInlineAttachment::getVerticalAlignment() const {
    return _verticalAlignment;
}

void TextInlineAttachment::setVerticalAlignment(InlineViewVerticalAlignment verticalAlignment) {
    _verticalAlignment = verticalAlignment;
}

} // namespace Valdi
