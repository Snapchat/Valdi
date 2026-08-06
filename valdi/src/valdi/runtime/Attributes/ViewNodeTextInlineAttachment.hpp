//
//  ViewNodeTextInlineAttachment.hpp
//  valdi
//

#pragma once

#include "valdi_core/cpp/Attributes/TextInlineAttachment.hpp"

namespace Valdi {

class ViewNode;

/**
 * Text inline attachment backed by a concrete ViewNode child.
 *
 * The attributed text parser only needs child indexes and attachment objects;
 * this subclass keeps the ViewNode lookup at the binding/runtime boundary and
 * exposes the child's current measured frame as the inline attachment size.
 */
class ViewNodeTextInlineAttachment final : public TextInlineAttachment {
public:
    ViewNodeTextInlineAttachment(size_t childIndex, Ref<ViewNode> viewNode);
    ~ViewNodeTextInlineAttachment() override;

    Size getSize() const override;

private:
    Ref<ViewNode> _viewNode;
};

} // namespace Valdi
