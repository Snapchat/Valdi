//
//  YogaAttributes.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/13/18.
//

#pragma once

#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Attributes/CompositeAttribute.hpp"
#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"

#include "valdi/runtime/Attributes/Yoga/YogaAttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/Yoga/YogaGetterSetterAttributeHandlerDelegate.hpp"

#include <fmt/format.h>
#include <memory>
#include <yoga/Yoga.h>
#include <yoga/numeric/FloatOptional.h>

struct YGConfig;

namespace Valdi {

using YGFloatOptional = facebook::yoga::FloatOptional;

class YogaAttributeHandlerDelegate;

class YogaAttributes : public SimpleRefCountable {
public:
    YogaAttributes(YGConfig* yogaConfig, AttributeIds& attributeIds, float pointScale);
    ~YogaAttributes() override;

    void bind(AttributeHandlerById& attributes);

private:
    YGNodeRef _defaultYogaNode;
    AttributeIds& _attributeIds;
    float _pointScale;

    FlatMap<StringBox, int> _directionToEnum;
    FlatMap<StringBox, int> _flexDirectionToEnum;
    FlatMap<StringBox, int> _justifyToEnum;
    FlatMap<StringBox, int> _alignToEnum;
    FlatMap<StringBox, int> _positionTypeToEnum;
    FlatMap<StringBox, int> _wrapToEnum;
    FlatMap<StringBox, int> _overflowToEnum;
    FlatMap<StringBox, int> _displayToEnum;

    friend YogaAttributeHandlerDelegate;

    void bindAttribute(const char* name,
                       bool isForChildrenNode,
                       AttributeHandlerById& attributes,
                       const Ref<YogaAttributeHandlerDelegate>& delegate);

    void bindEnumAttribute(const char* name,
                           bool isForChildrenNode,
                           AttributeHandlerById& attributes,
                           const FlatMap<StringBox, int>& associateMap,
                           YGNodeValueGetterSetter<int> getterSetter);

    void bindYGOptionalAttribute(const char* name,
                                 bool isForChildrenNode,
                                 AttributeHandlerById& attributes,
                                 YGNodeValueGetterSetter<YGFloatOptional> getterSetter);

    void bindYGStyleLengthAttribute(const char* name,
                                    bool isForChildrenNode,
                                    AttributeHandlerById& attributes,
                                    YGNodeValueGetterSetter<facebook::yoga::StyleLength> getterSetter);

    void bindYGStyleSizeLengthAttribute(const char* name,
                                        bool isForChildrenNode,
                                        AttributeHandlerById& attributes,
                                        YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength> getterSetter);

    template<typename SetEdges>
    void bindPaddingOrMarginAttributes(std::string_view name,
                                       bool isForChildrenNode,
                                       AttributeHandlerById& attributes,
                                       SetEdges&& setEdges);

    void bindEdgeAttributes(std::string_view name,
                            bool isForChildrenNode,
                            AttributeHandlerById& attributes,
                            const Ref<YogaAttributeHandlerDelegate>& delegate);

    void bindPositionAttributes(AttributeHandlerById& attributes);

    void configureDelegate(bool isForChildrenNode, const Ref<YogaAttributeHandlerDelegate>& delegate);
};

} // namespace Valdi
