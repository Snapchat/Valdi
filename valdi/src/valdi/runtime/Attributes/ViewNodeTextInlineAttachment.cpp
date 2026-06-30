//
//  ViewNodeTextInlineAttachment.cpp
//  valdi
//

#include "valdi/runtime/Attributes/ViewNodeTextInlineAttachment.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"
#include "utils/debugging/Assert.hpp"

#include <utility>

namespace Valdi {

ViewNodeTextInlineAttachment::ViewNodeTextInlineAttachment(size_t childIndex, Ref<ViewNode> viewNode)
    : TextInlineAttachment(childIndex), _viewNode(std::move(viewNode)) {
    SC_ASSERT(_viewNode != nullptr);
}

ViewNodeTextInlineAttachment::~ViewNodeTextInlineAttachment() = default;

Size ViewNodeTextInlineAttachment::getSize() const {
    return _viewNode->getMeasuredFrame().size();
}

} // namespace Valdi
