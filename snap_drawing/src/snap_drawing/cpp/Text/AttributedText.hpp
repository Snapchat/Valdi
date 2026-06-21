//
//  AttributedText.hpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/30/20.
//

#pragma once

#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include "snap_drawing/cpp/Touches/GestureRecognizer.hpp"

#include "snap_drawing/cpp/Text/Font.hpp"
#include "snap_drawing/cpp/Text/TextLayout.hpp"
#include "snap_drawing/cpp/Utils/Color.hpp"

#include <optional>
#include <vector>

namespace snap::drawing {

class AttributedTextOnTapAttribute : public Valdi::SimpleRefCountable {
public:
    virtual void onTap(const GestureRecognizer& gestureRecognizer,
                       GestureRecognizerState state,
                       const Point& location) = 0;
};

struct AttributedTextPartStyle {
    Ref<Font> font;
    std::optional<Color> color;
    std::optional<Color> backgroundColor;
    std::optional<TextBackgroundPadding> backgroundPadding;
    std::optional<BorderRadius> backgroundBorderRadius;
    std::optional<TextDecoration> textDecoration;
    std::optional<Valdi::TextAnimationTransform> animationTransform;
    Ref<AttributedTextOnTapAttribute> onTap;
};

class AttributedText : public Valdi::SimpleRefCountable, public Valdi::TextAttributeValueBase<AttributedTextPartStyle> {
public:
    explicit AttributedText(AttributedText::Parts parts);

    ~AttributedText() override;
};

} // namespace snap::drawing
