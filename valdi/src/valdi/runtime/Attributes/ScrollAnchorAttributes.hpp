//
//  ScrollAnchorAttributes.hpp
//  valdi
//

#pragma once

#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "valdi/runtime/Utils/SharedContainers.hpp"

namespace Valdi {

class AttributeIds;

class ScrollAnchorAttributes : public SimpleRefCountable {
public:
    explicit ScrollAnchorAttributes(AttributeIds& attributeIds);
    ~ScrollAnchorAttributes() override;

    void bind(AttributeHandlerById& attributes);

private:
    AttributeIds& _attributeIds;
    Shared<FlatMap<StringBox, int>> _scrollAnchorPositionMap;
};

} // namespace Valdi
