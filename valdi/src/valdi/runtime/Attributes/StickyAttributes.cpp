//
//  StickyAttributes.cpp
//  valdi
//

#include "valdi/runtime/Attributes/StickyAttributes.hpp"

#include "valdi/runtime/Attributes/AttributesBinderHelper.hpp"
#include "valdi/runtime/Context/StickyPosition.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"

namespace Valdi {

StickyAttributes::StickyAttributes(AttributeIds& attributeIds) : _attributeIds(attributeIds) {
    FlatMap<StringBox, int> stickyPositionMap(2);
    stickyPositionMap[STRING_LITERAL("none")] = StickyPositionNone;
    stickyPositionMap[STRING_LITERAL("top")] = StickyPositionTop;

    _stickyPositionMap = makeShared<FlatMap<StringBox, int>>(std::move(stickyPositionMap));
}

StickyAttributes::~StickyAttributes() = default;

void StickyAttributes::bind(AttributeHandlerById& attributes) {
    AttributesBinderHelper binder(_attributeIds, attributes);

    binder.bindViewNodeEnum("stickyPosition", &ViewNode::setStickyPosition, _stickyPositionMap, StickyPositionNone);
}

} // namespace Valdi
