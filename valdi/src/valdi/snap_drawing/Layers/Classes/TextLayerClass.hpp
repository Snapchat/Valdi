//
//  TextLayerClass.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 1/12/22.
//

#pragma once

#include "snap_drawing/cpp/Layers/TextLayer.hpp"
#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Layers/Interfaces/ILayerClass.hpp"

namespace snap::drawing {

class TextLayerClass : public ILayerClass {
public:
    TextLayerClass(const Ref<Resources>& resources, const Ref<LayerClass>& parentClass);
    ~TextLayerClass() override;

    Valdi::Ref<Layer> instantiate() override;

    bool managesChildFrames() const override;

    Size onMeasure(const Valdi::Value& attributes, Size maxSize, bool isRightToLeft) override;

    void bindAttributes(Valdi::AttributesBindingContext& binder) override;

    Valdi::Result<Valdi::Void> applyTextAttribute(TextLayer& textLayer, const Valdi::Value& value);
    void resetTextAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyFontAttribute(TextLayer& textLayer, const Valdi::Value& value);
    void resetFontAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyColorAttribute(TextLayer& textLayer, int64_t value);
    void resetColorAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyTextAlignAttribute(TextLayer& textLayer, const String& value);
    void resetTextAlignAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyTextDecorationAttribute(TextLayer& textLayer, const String& value);
    void resetTextDecorationAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyCustomUnderlineStyleAttribute(TextLayer& textLayer, const Valdi::Value& value);
    void resetCustomUnderlineStyleAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyTextOverflowAttribute(TextLayer& textLayer, const String& value);
    void resetTextOverflowAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyNumberOfLinesAttribute(TextLayer& textLayer, int64_t value);
    void resetNumberOfLinesAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyAdjustsFontSizeToFitWidthAttribute(TextLayer& textLayer, bool value);
    void resetAdjustsFontSizeToFitWidthAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyMinimumScaleFactorAttribute(TextLayer& textLayer, double value);
    void resetMinimumScaleFactorAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyLineHeightMultipleAttribute(TextLayer& textLayer, double value);
    void resetLineHeightMultipleAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyLineHeightAttribute(TextLayer& textLayer, double value);
    void resetLineHeightAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyLetterSpacingAttribute(TextLayer& textLayer, double value);
    void resetLetterSpacingAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyTextShadowAttribute(TextLayer& textLayer, const Valdi::Value& value);
    void resetTextShadowAttribute(TextLayer& textLayer);

    Valdi::Result<Valdi::Void> applyTextGradientAttribute(TextLayer& textLayer, const Valdi::Value& value);
    void resetTextGradientAttribute(TextLayer& textLayer);

    DECLARE_TEXT_ATTRIBUTE(TextLayer, value)

    DECLARE_INT_ATTRIBUTE(TextLayer, numberOfLines)

    DECLARE_COLOR_ATTRIBUTE(TextLayer, color)

    DECLARE_UNTYPED_ATTRIBUTE(TextLayer, font)

    DECLARE_STRING_ATTRIBUTE(TextLayer, textAlign)

    DECLARE_STRING_ATTRIBUTE(TextLayer, textDecoration)

    DECLARE_UNTYPED_ATTRIBUTE(TextLayer, customUnderlineStyle)

    DECLARE_STRING_ATTRIBUTE(TextLayer, textOverflow)

    DECLARE_BOOLEAN_ATTRIBUTE(TextLayer, adjustsFontSizeToFitWidth)

    DECLARE_DOUBLE_ATTRIBUTE(TextLayer, minimumScaleFactor)

    DECLARE_DOUBLE_ATTRIBUTE(TextLayer, lineHeightMultiple)

    DECLARE_DOUBLE_ATTRIBUTE(TextLayer, lineHeight)

    DECLARE_DOUBLE_ATTRIBUTE(TextLayer, letterSpacing)

    DECLARE_UNTYPED_ATTRIBUTE(TextLayer, textShadow)

    DECLARE_UNTYPED_ATTRIBUTE(TextLayer, textGradient)

    DECLARE_BOOLEAN_ATTRIBUTE(TextLayer, selectable)

    DECLARE_UNTYPED_ATTRIBUTE(TextLayer, selection)

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onSelectionChange(TextLayer& textLayer,
                                                       const Valdi::Ref<Valdi::ValueFunction>& value,
                                                       const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onSelectionChange(TextLayer& textLayer, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Value> preprocess_font(const Valdi::Value& value);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Value> preprocess_customUnderlineStyle(const Valdi::Value& value);
};

} // namespace snap::drawing
