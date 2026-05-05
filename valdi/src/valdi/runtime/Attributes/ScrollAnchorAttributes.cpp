//
//  ScrollAnchorAttributes.cpp
//  valdi
//

#include "valdi/runtime/Attributes/ScrollAnchorAttributes.hpp"

#include "valdi/runtime/Attributes/AttributesBinderHelper.hpp"
#include "valdi/runtime/Context/ScrollAnchorPosition.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"

namespace Valdi {

ScrollAnchorAttributes::ScrollAnchorAttributes(AttributeIds& attributeIds) : _attributeIds(attributeIds) {
    FlatMap<StringBox, int> scrollAnchorPositionMap(3);
    scrollAnchorPositionMap[STRING_LITERAL("none")] = ScrollAnchorPositionNone;
    scrollAnchorPositionMap[STRING_LITERAL("top")] = ScrollAnchorPositionTop;
    scrollAnchorPositionMap[STRING_LITERAL("bottom")] = ScrollAnchorPositionBottom;

    _scrollAnchorPositionMap = makeShared<FlatMap<StringBox, int>>(std::move(scrollAnchorPositionMap));
}

ScrollAnchorAttributes::~ScrollAnchorAttributes() = default;

void ScrollAnchorAttributes::bind(AttributeHandlerById& attributes) {
    AttributesBinderHelper binder(_attributeIds, attributes);

    binder.bindViewNodeEnum(
        "scrollAnchorPosition", &ViewNode::setScrollAnchorPosition, _scrollAnchorPositionMap, ScrollAnchorPositionNone);
}

} // namespace Valdi
