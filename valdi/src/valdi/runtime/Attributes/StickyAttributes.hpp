//
//  StickyAttributes.hpp
//  valdi
//

#pragma once

#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "valdi/runtime/Utils/SharedContainers.hpp"

namespace Valdi {

class AttributeIds;

class StickyAttributes : public SimpleRefCountable {
public:
    explicit StickyAttributes(AttributeIds& attributeIds);
    ~StickyAttributes() override;

    void bind(AttributeHandlerById& attributes);

private:
    AttributeIds& _attributeIds;
    Shared<FlatMap<StringBox, int>> _stickyPositionMap;
};

} // namespace Valdi
