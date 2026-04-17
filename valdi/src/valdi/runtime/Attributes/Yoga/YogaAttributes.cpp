//
//  YogaAttributes.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/13/18.
//

#include "valdi/runtime/Attributes/Yoga/YogaAttributes.hpp"
#include "valdi/runtime/Attributes/AttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/CompositeAttributeUtils.hpp"

#include "valdi/runtime/Attributes/Yoga/YGEdgesAttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/Yoga/YGEnumAttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/Yoga/YGFloatOptionalAttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/Yoga/YGValueAttributeHandlerDelegate.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"

#include <array>
#include <initializer_list>
#include <yoga/enums/Align.h>
#include <yoga/enums/Direction.h>
#include <yoga/enums/Display.h>
#include <yoga/enums/FlexDirection.h>
#include <yoga/enums/Justify.h>
#include <yoga/enums/Overflow.h>
#include <yoga/enums/PositionType.h>
#include <yoga/enums/Wrap.h>
#include <yoga/node/Node.h>

namespace Valdi {

template<typename Type, typename ToStringFunction>
static void populateAssociativeEnumMap(FlatMap<StringBox, int>& associateMap,
                                       std::initializer_list<Type> enumValues,
                                       ToStringFunction toStringFunction) {
    associateMap.reserve(enumValues.size());
    for (auto enumValue : enumValues) {
        associateMap[STRING_LITERAL(toStringFunction(enumValue))] = static_cast<int>(enumValue);
    }
}

template<typename Type>
static int toAttributeEnum(Type value) {
    return static_cast<int>(value);
}

template<typename Type>
static Type fromAttributeEnum(int value) {
    return static_cast<Type>(value);
}

static void setYGPosition(facebook::yoga::Style& style, YGEdge edge, facebook::yoga::StyleLength value) {
    style.setPosition(facebook::yoga::scopedEnum(edge), value);
}

static void setYGMargin(facebook::yoga::Style& style, YGEdge edge, facebook::yoga::StyleLength value) {
    style.setMargin(facebook::yoga::scopedEnum(edge), value);
}

static void setYGPadding(facebook::yoga::Style& style, YGEdge edge, facebook::yoga::StyleLength value) {
    style.setPadding(facebook::yoga::scopedEnum(edge), value);
}

static void setYGBorder(facebook::yoga::Style& style, YGEdge edge, facebook::yoga::StyleLength value) {
    style.setBorder(facebook::yoga::scopedEnum(edge), value);
}

static void setYGFlexBasis(facebook::yoga::Style& style, facebook::yoga::StyleSizeLength value) {
    style.setFlexBasis(value);
}

YogaAttributes::YogaAttributes(YGConfig* const yogaConfig, AttributeIds& attributeIds, float pointScale)
    : _defaultYogaNode(Yoga::createNode(yogaConfig)), _attributeIds(attributeIds), _pointScale(pointScale) {
    populateAssociativeEnumMap<facebook::yoga::Direction>(
        _directionToEnum,
        {facebook::yoga::Direction::Inherit, facebook::yoga::Direction::LTR, facebook::yoga::Direction::RTL},
        [](facebook::yoga::Direction value) { return facebook::yoga::toString(value); });
    populateAssociativeEnumMap<facebook::yoga::FlexDirection>(
        _flexDirectionToEnum,
        {facebook::yoga::FlexDirection::Column,
         facebook::yoga::FlexDirection::ColumnReverse,
         facebook::yoga::FlexDirection::Row,
         facebook::yoga::FlexDirection::RowReverse},
        [](facebook::yoga::FlexDirection value) { return facebook::yoga::toString(value); });
    populateAssociativeEnumMap<facebook::yoga::Justify>(
        _justifyToEnum,
        {facebook::yoga::Justify::Auto,
         facebook::yoga::Justify::FlexStart,
         facebook::yoga::Justify::Center,
         facebook::yoga::Justify::FlexEnd,
         facebook::yoga::Justify::SpaceBetween,
         facebook::yoga::Justify::SpaceAround,
         facebook::yoga::Justify::SpaceEvenly,
         facebook::yoga::Justify::Stretch,
         facebook::yoga::Justify::Start,
         facebook::yoga::Justify::End},
        [](facebook::yoga::Justify value) { return facebook::yoga::toString(value); });
    populateAssociativeEnumMap<facebook::yoga::Align>(
        _alignToEnum,
        {facebook::yoga::Align::Auto,
         facebook::yoga::Align::FlexStart,
         facebook::yoga::Align::Center,
         facebook::yoga::Align::FlexEnd,
         facebook::yoga::Align::Stretch,
         facebook::yoga::Align::Baseline,
         facebook::yoga::Align::SpaceBetween,
         facebook::yoga::Align::SpaceAround,
         facebook::yoga::Align::SpaceEvenly,
         facebook::yoga::Align::Start,
         facebook::yoga::Align::End},
        [](facebook::yoga::Align value) { return facebook::yoga::toString(value); });
    populateAssociativeEnumMap<facebook::yoga::PositionType>(
        _positionTypeToEnum,
        {facebook::yoga::PositionType::Static,
         facebook::yoga::PositionType::Relative,
         facebook::yoga::PositionType::Absolute},
        [](facebook::yoga::PositionType value) { return facebook::yoga::toString(value); });
    populateAssociativeEnumMap<facebook::yoga::Wrap>(
        _wrapToEnum,
        {facebook::yoga::Wrap::NoWrap, facebook::yoga::Wrap::Wrap, facebook::yoga::Wrap::WrapReverse},
        [](facebook::yoga::Wrap value) { return facebook::yoga::toString(value); });
    populateAssociativeEnumMap<facebook::yoga::Overflow>(
        _overflowToEnum,
        {facebook::yoga::Overflow::Visible, facebook::yoga::Overflow::Hidden, facebook::yoga::Overflow::Scroll},
        [](facebook::yoga::Overflow value) { return facebook::yoga::toString(value); });
    populateAssociativeEnumMap<facebook::yoga::Display>(
        _displayToEnum,
        {facebook::yoga::Display::Flex,
         facebook::yoga::Display::None,
         facebook::yoga::Display::Contents,
         facebook::yoga::Display::Grid},
        [](facebook::yoga::Display value) { return facebook::yoga::toString(value); });
}

YogaAttributes::~YogaAttributes() {
    YGNodeFree(_defaultYogaNode);
}

void YogaAttributes::bindAttribute(const char* name,
                                   bool isForChildrenNode,
                                   AttributeHandlerById& attributes,
                                   const Ref<YogaAttributeHandlerDelegate>& delegate) {
    configureDelegate(isForChildrenNode, delegate);
    auto attributeName = StringCache::getGlobal().makeStringFromLiteral(name);
    auto attributeId = _attributeIds.getIdForName(attributeName);
    attributes[attributeId] = AttributeHandler(attributeId, attributeName, delegate, nullptr, false, false);
}

template<YGEdge edge>
static YGNodeValueGetterSetter<facebook::yoga::StyleLength> makePositionEdgeGetterSetter() {
    return YGNodeValueGetterSetter<facebook::yoga::StyleLength>(
        [](facebook::yoga::Style& style) -> facebook::yoga::StyleLength {
            return style.position(facebook::yoga::scopedEnum(edge));
        },
        [](facebook::yoga::Style& style, facebook::yoga::StyleLength value) { setYGPosition(style, edge, value); });
}

void YogaAttributes::bindPositionAttributes(AttributeHandlerById& attributes) {
    bindYGStyleLengthAttribute("top", false, attributes, makePositionEdgeGetterSetter<YGEdgeTop>());
    bindYGStyleLengthAttribute("right", false, attributes, makePositionEdgeGetterSetter<YGEdgeEnd>());
    bindYGStyleLengthAttribute("bottom", false, attributes, makePositionEdgeGetterSetter<YGEdgeBottom>());
    bindYGStyleLengthAttribute("left", false, attributes, makePositionEdgeGetterSetter<YGEdgeStart>());
}

static std::array<std::pair<YGEdge, std::string>, 4> getAllEdges() {
    return {
        std::make_pair(YGEdgeTop, "top"),
        std::make_pair(YGEdgeEnd, "right"),
        std::make_pair(YGEdgeBottom, "bottom"),
        std::make_pair(YGEdgeStart, "left"),
    };
}

void YogaAttributes::bindEnumAttribute(const char* name,
                                       bool isForChildrenNode,
                                       AttributeHandlerById& attributes,
                                       const FlatMap<StringBox, int>& associateMap,
                                       YGNodeValueGetterSetter<int> getterSetter) {
    bindAttribute(
        name, isForChildrenNode, attributes, makeShared<YGEnumAttributeHandlerDelegate>(associateMap, getterSetter));
}

void YogaAttributes::bindYGOptionalAttribute(const char* name,
                                             bool isForChildrenNode,
                                             AttributeHandlerById& attributes,
                                             YGNodeValueGetterSetter<YGFloatOptional> getterSetter) {
    bindAttribute(
        name, isForChildrenNode, attributes, makeShared<YGFloatOptionalAttributeHandlerDelegate>(getterSetter));
}

void YogaAttributes::bindYGStyleLengthAttribute(const char* name,
                                                bool isForChildrenNode,
                                                AttributeHandlerById& attributes,
                                                YGNodeValueGetterSetter<facebook::yoga::StyleLength> getterSetter) {
    bindAttribute(name, isForChildrenNode, attributes, makeShared<YGStyleLengthAttributeHandlerDelegate>(getterSetter));
}

void YogaAttributes::bindYGStyleSizeLengthAttribute(
    const char* name,
    bool isForChildrenNode,
    AttributeHandlerById& attributes,
    YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength> getterSetter) {
    bindAttribute(
        name, isForChildrenNode, attributes, makeShared<YGStyleSizeLengthAttributeHandlerDelegate>(getterSetter));
}

template<typename SetEdges>
void YogaAttributes::bindPaddingOrMarginAttributes(std::string_view name,
                                                   bool isForChildrenNode,
                                                   AttributeHandlerById& attributes,
                                                   SetEdges&& setEdges) {
    bindEdgeAttributes(name,
                       isForChildrenNode,
                       attributes,
                       makeShared<YGEdgesAttributeHandlerDelegate<SetEdges>>(std::forward<SetEdges>(setEdges)));
}

void YogaAttributes::bindEdgeAttributes(std::string_view name,
                                        bool isForChildrenNode,
                                        AttributeHandlerById& attributes,
                                        const Ref<YogaAttributeHandlerDelegate>& delegate) {
    std::vector<CompositeAttributePart> parts;

    parts.emplace_back(_attributeIds.getIdForName(name), true);

    for (const auto& edge : getAllEdges()) {
        auto correctedEdge = edge.second;
        correctedEdge[0] = toupper(correctedEdge[0]);
        auto attributeName = std::string(name) + correctedEdge;

        auto attributeId = _attributeIds.getIdForName(StringCache::getGlobal().makeString(std::move(attributeName)));
        parts.emplace_back(attributeId, true);
    }

    auto compositeName = StringCache::getGlobal().makeString(std::string("_") + std::string(name));
    auto compositeId = _attributeIds.getIdForName(compositeName);

    auto compositeAttribute = Valdi::makeShared<CompositeAttribute>(compositeId, compositeName, std::move(parts));

    configureDelegate(isForChildrenNode, delegate);
    Valdi::bindCompositeAttribute(_attributeIds, attributes, compositeAttribute, delegate, false);
}

void YogaAttributes::configureDelegate(bool isForChildrenNode, const Ref<YogaAttributeHandlerDelegate>& delegate) {
    delegate->_isForChildrenNode = isForChildrenNode;
    delegate->_yogaAttributes = strongSmallRef(this);
}

void YogaAttributes::bind(AttributeHandlerById& attributes) {
    bindEnumAttribute(
        "direction",
        true,
        attributes,
        _directionToEnum,
        YGNodeValueGetterSetter<int>(
            [](facebook::yoga::Style& style) { return toAttributeEnum(style.direction()); },
            [](facebook::yoga::Style& style, int enumValue) {
                style.setDirection(fromAttributeEnum<facebook::yoga::Direction>(enumValue));
            }));

    bindEnumAttribute(
        "flexDirection",
        true,
        attributes,
        _flexDirectionToEnum,
        YGNodeValueGetterSetter<int>([](facebook::yoga::Style& style) {
                                         return toAttributeEnum(style.flexDirection());
                                     },
                                     [](facebook::yoga::Style& style, int enumValue) {
                                         style.setFlexDirection(
                                             fromAttributeEnum<facebook::yoga::FlexDirection>(enumValue));
                                     }));

    bindEnumAttribute(
        "justifyContent",
        true,
        attributes,
        _justifyToEnum,
        YGNodeValueGetterSetter<int>([](facebook::yoga::Style& style) {
                                         return toAttributeEnum(style.justifyContent());
                                     },
                                     [](facebook::yoga::Style& style, int enumValue) {
                                         style.setJustifyContent(
                                             fromAttributeEnum<facebook::yoga::Justify>(enumValue));
                                     }));

    bindEnumAttribute(
        "alignContent",
        true,
        attributes,
        _alignToEnum,
        YGNodeValueGetterSetter<int>(
            [](facebook::yoga::Style& style) {
                return toAttributeEnum(style.alignContent());
            },
            [](facebook::yoga::Style& style, int enumValue) {
                style.setAlignContent(fromAttributeEnum<facebook::yoga::Align>(enumValue));
            }));

    bindEnumAttribute(
        "alignItems",
        true,
        attributes,
        _alignToEnum,
        YGNodeValueGetterSetter<int>(
            [](facebook::yoga::Style& style) {
                return toAttributeEnum(style.alignItems());
            },
            [](facebook::yoga::Style& style, int enumValue) {
                style.setAlignItems(fromAttributeEnum<facebook::yoga::Align>(enumValue));
            }));

    bindEnumAttribute(
        "alignSelf",
        false,
        attributes,
        _alignToEnum,
        YGNodeValueGetterSetter<int>(
            [](facebook::yoga::Style& style) {
                return toAttributeEnum(style.alignSelf());
            },
            [](facebook::yoga::Style& style, int enumValue) {
                style.setAlignSelf(fromAttributeEnum<facebook::yoga::Align>(enumValue));
            }));

    bindEnumAttribute(
        "position",
        false,
        attributes,
        _positionTypeToEnum,
        YGNodeValueGetterSetter<int>([](facebook::yoga::Style& style) {
                                         return toAttributeEnum(style.positionType());
                                     },
                                     [](facebook::yoga::Style& style, int enumValue) {
                                         style.setPositionType(
                                             fromAttributeEnum<facebook::yoga::PositionType>(enumValue));
                                     }));

    bindEnumAttribute(
        "flexWrap",
        true,
        attributes,
        _wrapToEnum,
        YGNodeValueGetterSetter<int>(
            [](facebook::yoga::Style& style) {
                return toAttributeEnum(style.flexWrap());
            },
            [](facebook::yoga::Style& style, int enumValue) {
                style.setFlexWrap(fromAttributeEnum<facebook::yoga::Wrap>(enumValue));
            }));

    bindEnumAttribute(
        "overflow",
        true,
        attributes,
        _overflowToEnum,
        YGNodeValueGetterSetter<int>(
            [](facebook::yoga::Style& style) {
                return toAttributeEnum(style.overflow());
            },
            [](facebook::yoga::Style& style, int enumValue) {
                style.setOverflow(fromAttributeEnum<facebook::yoga::Overflow>(enumValue));
            }));

    bindEnumAttribute(
        "display",
        false,
        attributes,
        _displayToEnum,
        YGNodeValueGetterSetter<int>(
            [](facebook::yoga::Style& style) {
                return toAttributeEnum(style.display());
            },
            [](facebook::yoga::Style& style, int enumValue) {
                style.setDisplay(fromAttributeEnum<facebook::yoga::Display>(enumValue));
            }));

    bindYGOptionalAttribute("flexGrow",
                            false,
                            attributes,
                            YGNodeValueGetterSetter<YGFloatOptional>(
                                [](facebook::yoga::Style& style) -> YGFloatOptional {
                                    return style.flexGrow();
                                },
                                [](facebook::yoga::Style& style, YGFloatOptional value) {
                                    style.setFlexGrow(value);
                                }));

    bindYGOptionalAttribute("flexShrink",
                            false,
                            attributes,
                            YGNodeValueGetterSetter<YGFloatOptional>(
                                [](facebook::yoga::Style& style) -> YGFloatOptional {
                                    return style.flexShrink();
                                },
                                [](facebook::yoga::Style& style, YGFloatOptional value) {
                                    style.setFlexShrink(value);
                                }));

    bindYGStyleSizeLengthAttribute("flexBasis",
                                   false,
                                   attributes,
                                   YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength>(
                                       [](facebook::yoga::Style& style) -> facebook::yoga::StyleSizeLength {
                                           return style.flexBasis();
                                       },
                                       [](facebook::yoga::Style& style, facebook::yoga::StyleSizeLength value) {
                                           setYGFlexBasis(style, value);
                                       }));

    bindPositionAttributes(attributes);

    bindPaddingOrMarginAttributes("margin", false, attributes, [](facebook::yoga::Style& style,
                                                                  const facebook::yoga::StyleLength& top,
                                                                  const facebook::yoga::StyleLength& end,
                                                                  const facebook::yoga::StyleLength& bottom,
                                                                  const facebook::yoga::StyleLength& start) {
        setYGMargin(style, YGEdgeTop, top);
        setYGMargin(style, YGEdgeEnd, end);
        setYGMargin(style, YGEdgeBottom, bottom);
        setYGMargin(style, YGEdgeStart, start);
    });
    bindPaddingOrMarginAttributes("padding", true, attributes, [](facebook::yoga::Style& style,
                                                                  const facebook::yoga::StyleLength& top,
                                                                  const facebook::yoga::StyleLength& end,
                                                                  const facebook::yoga::StyleLength& bottom,
                                                                  const facebook::yoga::StyleLength& start) {
        setYGPadding(style, YGEdgeTop, top);
        setYGPadding(style, YGEdgeEnd, end);
        setYGPadding(style, YGEdgeBottom, bottom);
        setYGPadding(style, YGEdgeStart, start);
    });

    bindYGStyleLengthAttribute(
        "borderTopWidth",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleLength {
                return style.border(facebook::yoga::scopedEnum(YGEdgeTop));
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleLength value) {
                setYGBorder(style, YGEdgeTop, value);
            }));

    bindYGStyleLengthAttribute(
        "borderRightWidth",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleLength {
                return style.border(facebook::yoga::scopedEnum(YGEdgeEnd));
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleLength value) {
                setYGBorder(style, YGEdgeEnd, value);
            }));

    bindYGStyleLengthAttribute(
        "borderBottomWidth",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleLength {
                return style.border(facebook::yoga::scopedEnum(YGEdgeBottom));
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleLength value) {
                setYGBorder(style, YGEdgeBottom, value);
            }));

    bindYGStyleLengthAttribute(
        "borderLeftWidth",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleLength {
                return style.border(facebook::yoga::scopedEnum(YGEdgeStart));
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleLength value) {
                setYGBorder(style, YGEdgeStart, value);
            }));

    bindYGStyleLengthAttribute(
        "borderWidth",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleLength {
                return style.border(facebook::yoga::scopedEnum(YGEdgeAll));
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleLength value) {
                setYGBorder(style, YGEdgeAll, value);
            }));

    bindYGStyleSizeLengthAttribute(
        "width",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleSizeLength {
                return style.dimension(facebook::yoga::Dimension::Width);
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleSizeLength value) {
                style.setDimension(facebook::yoga::Dimension::Width, value);
            }));

    bindYGStyleSizeLengthAttribute(
        "height",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleSizeLength {
                return style.dimension(facebook::yoga::Dimension::Height);
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleSizeLength value) {
                style.setDimension(facebook::yoga::Dimension::Height, value);
            }));

    bindYGStyleSizeLengthAttribute(
        "minWidth",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleSizeLength {
                return style.minDimension(facebook::yoga::Dimension::Width);
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleSizeLength value) {
                style.setMinDimension(facebook::yoga::Dimension::Width, value);
            }));

    bindYGStyleSizeLengthAttribute(
        "minHeight",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleSizeLength {
                return style.minDimension(facebook::yoga::Dimension::Height);
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleSizeLength value) {
                style.setMinDimension(facebook::yoga::Dimension::Height, value);
            }));

    bindYGStyleSizeLengthAttribute(
        "maxWidth",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleSizeLength {
                return style.maxDimension(facebook::yoga::Dimension::Width);
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleSizeLength value) {
                style.setMaxDimension(facebook::yoga::Dimension::Width, value);
            }));

    bindYGStyleSizeLengthAttribute(
        "maxHeight",
        false,
        attributes,
        YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength>(
            [](facebook::yoga::Style& style) -> facebook::yoga::StyleSizeLength {
                return style.maxDimension(facebook::yoga::Dimension::Height);
            },
            [](facebook::yoga::Style& style, facebook::yoga::StyleSizeLength value) {
                style.setMaxDimension(facebook::yoga::Dimension::Height, value);
            }));

    bindYGOptionalAttribute("aspectRatio",
                            false,
                            attributes,
                            YGNodeValueGetterSetter<YGFloatOptional>(
                                [](facebook::yoga::Style& style) -> YGFloatOptional {
                                    return style.aspectRatio();
                                },
                                [](facebook::yoga::Style& style, YGFloatOptional value) {
                                    if (!value.isUndefined() && value.unwrap() == 0) {
                                        // safety for aspectRatio 0, we make it undefined instead.
                                        style.setAspectRatio(YGFloatOptional());
                                    } else {
                                        style.setAspectRatio(value);
                                    }
                                }));
}

} // namespace Valdi
