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

    Size onMeasure(const Valdi::Value& attributes, Size maxSize, bool isRightToLeft) override;

    void bindAttributes(Valdi::AttributesBindingContext& binder) override;

    DECLARE_TEXT_ATTRIBUTE(TextLayer, value)

    DECLARE_INT_ATTRIBUTE(TextLayer, numberOfLines)

    DECLARE_COLOR_ATTRIBUTE(TextLayer, color)

    DECLARE_UNTYPED_ATTRIBUTE(TextLayer, font)

    DECLARE_STRING_ATTRIBUTE(TextLayer, textAlign)

    DECLARE_STRING_ATTRIBUTE(TextLayer, textDecoration)

    DECLARE_STRING_ATTRIBUTE(TextLayer, textOverflow)

    DECLARE_BOOLEAN_ATTRIBUTE(TextLayer, adjustsFontSizeToFitWidth)

    DECLARE_DOUBLE_ATTRIBUTE(TextLayer, minimumScaleFactor)

    DECLARE_DOUBLE_ATTRIBUTE(TextLayer, lineHeightMultiple)

    DECLARE_DOUBLE_ATTRIBUTE(TextLayer, lineHeight)

    DECLARE_DOUBLE_ATTRIBUTE(TextLayer, letterSpacing)

    DECLARE_UNTYPED_ATTRIBUTE(TextLayer, textShadow)

    DECLARE_UNTYPED_ATTRIBUTE(TextLayer, textGradient)

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Value> preprocess_font(const Valdi::Value& value);
};

} // namespace snap::drawing
