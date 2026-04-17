//
//  YGEdgesAttributeHandlerDelegate.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/3/21.
//

#pragma once

#include "valdi/runtime/Attributes/Yoga/YogaAttributeHandlerDelegate.hpp"

namespace facebook::yoga {
class Style;
}

namespace Valdi {

class YGEdgesBaseAttributeHandlerDelegate : public YogaAttributeHandlerDelegate {
public:
    Result<Void> onApply(YGNodeRef node, const Value& value) override;
    void onReset(YGNodeRef node, YGNodeRef defaultYogaNode) override;

protected:
    virtual void setEdges(facebook::yoga::Style& style,
                          const facebook::yoga::StyleLength& top,
                          const facebook::yoga::StyleLength& end,
                          const facebook::yoga::StyleLength& bottom,
                          const facebook::yoga::StyleLength& start) = 0;

private:
    Result<std::vector<facebook::yoga::StyleLength>> parseFlexBoxShorthand(const Value& value);
};

template<typename T>
class YGEdgesAttributeHandlerDelegate : public YGEdgesBaseAttributeHandlerDelegate {
public:
    explicit YGEdgesAttributeHandlerDelegate(T&& setEdges) : _setEdges(std::move(setEdges)) {}

protected:
    void setEdges(facebook::yoga::Style& style,
                  const facebook::yoga::StyleLength& top,
                  const facebook::yoga::StyleLength& end,
                  const facebook::yoga::StyleLength& bottom,
                  const facebook::yoga::StyleLength& start) override {
        _setEdges(style, top, end, bottom, start);
    }

private:
    T _setEdges;
};

} // namespace Valdi
