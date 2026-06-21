//
//  TextLayer.hpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/28/20.
//

#pragma once

#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Text/AttributedText.hpp"
#include "snap_drawing/cpp/Text/Font.hpp"
#include "snap_drawing/cpp/Text/TextLayout.hpp"

#include "valdi_core/cpp/Utils/PlatformResult.hpp"

namespace snap::drawing {

class FontManager;
class ValdiAnimator;
class AttributedTextOnTapGestureRecognizer;

struct TextShadow {
    Color color;
    Scalar radius;
    float opacity;
    Scalar offsetX;
    Scalar offsetY;

    constexpr TextShadow() : color(Color()), radius(0), opacity(0), offsetX(0), offsetY(0) {};
    constexpr TextShadow(Color color, Scalar radius, float opacity, Scalar offsetX, Scalar offsetY)
        : color(color), radius(radius), opacity(opacity), offsetX(offsetX), offsetY(offsetY) {}

    bool operator==(const TextShadow& other) const {
        return this->color == other.color && this->radius == other.radius && this->opacity == other.opacity &&
               this->offsetX == other.offsetX && this->offsetY == other.offsetY;
    }
};

class TextLayer : public Layer {
public:
    explicit TextLayer(const Ref<Resources>& resources);
    ~TextLayer() override;

    Size sizeThatFits(Size maxSize) override;

    void setText(const Valdi::StringBox& text);
    const Valdi::StringBox& getText() const;

    void setAttributedText(const Valdi::Ref<AttributedText>& attributedText);
    const Valdi::Ref<AttributedText>& getAttributedText() const;

    void setTextColor(Color textColor);
    Color getTextColor() const;

    void setTextAlign(TextAlign textAlign);
    TextAlign getTextAlign() const;

    void setTextDecoration(TextDecoration textDecoration);
    TextDecoration getTextDecoration() const;

    void setTextShadow(Color color, Scalar radius, float opacity, Scalar offsetX, Scalar offsetY);
    void resetTextShadow();
    TextShadow getTextShadow() const;

    void setAdjustsFontSizeToFitWidth(bool adjustsFontSizeToFitWidth);
    bool getAdjustsFontSizeToFitWidth() const;

    void setMinimumScaleFactor(double minimumScaleFactor);
    double getMinimumScaleFactor() const;

    void setLineHeight(Scalar lineHeight);
    void resetLineHeight();

    void setLineHeightMultiple(Scalar lineHeightMultiple);
    Scalar getLineHeightMultiple() const;

    void setLetterSpacing(Scalar letterSpacing);
    Scalar getLetterSpacing() const;

    void setTextFont(const Valdi::Ref<Font>& font);
    const Valdi::Ref<Font>& getTextFont() const;

    void setNumberOfLines(int numberOfLines);
    int getNumberOfLines() const;

    void setTextOverflow(TextOverflow textOveflow);
    TextOverflow getTextOverflow() const;

    void setTextLinearGradient(std::vector<Scalar>&& locations,
                               std::vector<Color>&& colors,
                               LinearGradientOrientation orientation);
    void setTextRadialGradient(std::vector<Scalar>&& locations, std::vector<Color>&& colors);
    void resetTextGradient();

    static Size measureText(Size maxSize,
                            const String& text,
                            const Ref<AttributedText>& attributedText,
                            const Ref<Font>& font,
                            TextAlign textAlign,
                            TextDecoration textDecoration,
                            TextOverflow textOverflow,
                            int numberOfLines,
                            TextLayoutLineHeight lineHeight,
                            Scalar letterSpacing,
                            bool isRightToLeft,
                            bool adjustsFontSizeToFitWidth,
                            double minimumScaleFactor,
                            bool respectDynamicType,
                            Scalar displayScale,
                            Scalar dynamicTypeScale,
                            const Ref<FontManager>& fontManager);

    static Ref<TextLayout> makeTextLayout(Size maxSize,
                                          const String& text,
                                          const Ref<AttributedText>& attributedText,
                                          const Ref<Font>& font,
                                          TextAlign textAlign,
                                          TextDecoration textDecoration,
                                          TextOverflow textOverflow,
                                          int numberOfLines,
                                          TextLayoutLineHeight lineHeight,
                                          Scalar letterSpacing,
                                          bool isRightToLeft,
                                          bool adjustsFontSizeToFitWidth,
                                          double minimumScaleFactor,
                                          bool respectDynamicType,
                                          bool includeTextBlob,
                                          Scalar displayScale,
                                          Scalar dynamicTypeScale,
                                          const Ref<FontManager>& fontManager);

    static Ref<TextLayout> makeTextLayoutUnscaled(Size maxSize,
                                                  const String& text,
                                                  const Ref<AttributedText>& attributedText,
                                                  const Ref<Font>& font,
                                                  TextAlign textAlign,
                                                  TextDecoration textDecoration,
                                                  TextOverflow textOverflow,
                                                  int numberOfLines,
                                                  TextLayoutLineHeight lineHeight,
                                                  Scalar letterSpacing,
                                                  bool isRightToLeft,
                                                  double fontScale,
                                                  bool respectDynamicType,
                                                  bool includeTextBlob,
                                                  Scalar displayScale,
                                                  Scalar dynamicTypeScale,
                                                  const Ref<FontManager>& fontManager);

protected:
    void onDraw(DrawingContext& drawingContext) override;
    void onRightToLeftChanged() override;

private:
    Valdi::Ref<Font> _textFont;
    Paint _textPaint;
    Valdi::StringBox _text;
    Valdi::Ref<AttributedText> _attributedText;
    Ref<AttributedTextOnTapGestureRecognizer> _attributedTextOnTapGestureRecognizer;
    TextAlign _textAlign = TextAlignLeft;
    TextDecoration _textDecoration = TextDecorationNone;
    TextShadow _textShadow;
    TextOverflow _textOverflow = TextOverflowEllipsis;
    int _numberOfLines = 1;
    bool _adjustsFontSizeToFitWidth = false;
    double _minimumScaleFactor = 0.0;
    bool _usesExplicitLineHeight = false;
    Scalar _lineHeight = 0.0f;
    Scalar _lineHeightMultipleValue = 1.0f;
    Scalar _letterSpacing = 0.0f;

    Ref<TextLayout> _textLayout;
    GradientWrapper _gradientWrapper;

    TextLayout& getTextLayout(Size size, bool respectDynamicType, Scalar displayScale, Scalar dynamicTypeScale);

    Scalar getTextVerticalOffset(const Size& layerSize, const TextLayout& layout, Scalar displayScale) const;
    TextLayoutLineHeight resolveLineHeight(Scalar displayScale) const;

    void setNeedsTextLayout();

    void removeOnTapGestureRecognizer();
    void addOnTapGestureRecognizer();

    void drawTextVisualEntriesShadows(DrawingContext& drawingContext,
                                      const std::vector<TextLayoutVisualEntry>& visualEntries);

    void drawTextVisualEntries(DrawingContext& drawingContext,
                               const std::vector<TextLayoutVisualEntry>& visualEntries,
                               bool predraw);

    void applyGradientToTextPaint(Paint& paint);

    void drawText(DrawingContext& DrawingContext, const sk_sp<SkTextBlob>& textBlob, std::optional<Color> textColor);

    bool hasTextShadow() const;

    void drawTextShadows(DrawingContext& drawingContext, const sk_sp<SkTextBlob>& textBlob);
};

} // namespace snap::drawing
