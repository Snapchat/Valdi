//
//  TextLayer.cpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/28/20.
//

#include "snap_drawing/cpp/Layers/TextLayer.hpp"

#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

#include "include/core/SkBlurTypes.h"
#include "include/core/SkMaskFilter.h"
#include "include/effects/SkGradientShader.h"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Text/TextLayoutBuilder.hpp"
#include "snap_drawing/cpp/Touches/AttributedTextOnTapGestureRecognizer.hpp"
#include "snap_drawing/cpp/Utils/GradientWrapper.hpp"
#include "snap_drawing/cpp/Utils/Path.hpp"

#include <algorithm>
#include <cmath>

namespace snap::drawing {

constexpr double kMaxAdjustsFontSizeToFitWidthAttempt = 8;

static Path makeTextDecorationPath(const Rect& bounds) {
    Path path;
    auto centerY = bounds.y() + bounds.height() / 2.0f;
    path.moveTo(bounds.x(), centerY);
    path.lineTo(bounds.right, centerY);
    return path;
}

static void drawTextDecoration(DrawingContext& drawingContext,
                               Paint& paint,
                               TextLayoutDecorationStyle style,
                               const std::optional<TextCustomUnderlineStyle>& customUnderlineStyle,
                               const Rect& bounds) {
    if (customUnderlineStyle) {
        paint.setStroke(true);
        paint.setStrokeWidth(customUnderlineStyle->height);
        paint.setStrokeCap(PaintStrokeCapButt);
        if (customUnderlineStyle->isPatterned()) {
            paint.setStrokeDashPattern(customUnderlineStyle->onWidth, customUnderlineStyle->offWidth);
        }

        auto path = makeTextDecorationPath(bounds);
        drawingContext.drawPaint(paint, path);
    } else if (style == TextLayoutDecorationStyleDashed) {
        auto strokeWidth = std::max<Scalar>(bounds.height(), 1.0f);
        paint.setStroke(true);
        paint.setStrokeWidth(strokeWidth);
        paint.setStrokeCap(PaintStrokeCapButt);
        paint.setStrokeDashPattern(strokeWidth * 3.0f, strokeWidth * 2.0f);

        auto path = makeTextDecorationPath(bounds);
        drawingContext.drawPaint(paint, path);
    } else if (style == TextLayoutDecorationStyleDotted) {
        auto strokeWidth = std::max<Scalar>(bounds.height(), 1.0f);
        paint.setStroke(false);
        paint.setStrokeDotPattern(strokeWidth / 2.0f, strokeWidth * 2.0f);

        auto path = makeTextDecorationPath(bounds);
        drawingContext.drawPaint(paint, path);
    } else {
        drawingContext.drawPaint(paint, bounds);
    }
}

TextLayer::TextLayer(const Ref<Resources>& resources) : Layer(resources) {
    _textPaint.setAntiAlias(true);
    _gradientWrapper = GradientWrapper();
}

TextLayer::~TextLayer() = default;

Size TextLayer::sizeThatFits(Size maxSize) {
    auto respectDynamicType = getResources()->getRespectDynamicType();
    auto displayScale = getResources()->getDisplayScale();
    auto dynamicTypeScale = getResources()->getDynamicTypeScale();
    auto& layout = getTextLayout(maxSize, respectDynamicType, displayScale, dynamicTypeScale);

    return Size::make(layout.getBounds().width() / displayScale, layout.getBounds().height() / displayScale);
}

void TextLayer::onDraw(DrawingContext& drawingContext) {
    Layer::onDraw(drawingContext);

    auto respectDynamicType = getResources()->getRespectDynamicType();
    auto displayScale = getResources()->getDisplayScale();
    auto dynamicTypeScale = getResources()->getDynamicTypeScale();
    auto layerSize = Size::make(getFrame().width(), getFrame().height());
    auto& layout = getTextLayout(layerSize, respectDynamicType, displayScale, dynamicTypeScale);
    auto offsetY = getTextVerticalOffset(layerSize, layout, displayScale);
    drawingContext.concat(Matrix::makeScaleTranslate(1.0f / displayScale, 1.0f / displayScale, 0.0f, offsetY));

    if (hasTextShadow()) {
        // Draw all of the shadows first
        drawTextVisualEntriesShadows(drawingContext, layout.getVisualEntries());
        for (const auto& entry : layout.getEntries()) {
            if (entry.textBlob != nullptr) {
                drawTextShadows(drawingContext, entry.textBlob);
            }
        }
    }

    // Then draw all of the text things
    drawTextVisualEntries(drawingContext, layout.getVisualEntries(), /* predraw */ true);

    for (const auto& entry : layout.getEntries()) {
        if (entry.textBlob != nullptr) {
            drawText(drawingContext, entry.textBlob, entry.color);
        }
    }

    drawTextVisualEntries(drawingContext, layout.getVisualEntries(), /* predraw */ false);
}

void TextLayer::drawTextVisualEntriesShadows(DrawingContext& drawingContext,
                                             const std::vector<TextLayoutVisualEntry>& visualEntries) {
    for (const auto& visualEntry : visualEntries) {
        if (visualEntry.kind == TextLayoutVisualEntryKindBackground) {
            continue;
        }

        auto resolvedPaint = _textPaint;
        resolvedPaint.getSkValue().setMaskFilter(
            SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle, _textShadow.radius, false));

        resolvedPaint.setColor(_textShadow.color);
        resolvedPaint.setAlpha(_textShadow.opacity);

        auto bounds = visualEntry.bounds;
        bounds.offsetX(_textShadow.offsetX);
        bounds.offsetY(_textShadow.offsetY);

        drawTextDecoration(drawingContext, resolvedPaint, visualEntry.style, visualEntry.customUnderlineStyle, bounds);
    }
}

void TextLayer::drawTextVisualEntries(DrawingContext& drawingContext,
                                      const std::vector<TextLayoutVisualEntry>& visualEntries,
                                      bool predraw) {
    for (const auto& visualEntry : visualEntries) {
        if (visualEntry.predraw != predraw) {
            continue;
        }

        auto resolvedPaint = _textPaint;

        if (visualEntry.kind == TextLayoutVisualEntryKindBackground) {
            if (!visualEntry.color) {
                continue;
            }
            resolvedPaint.setColor(visualEntry.color.value());
            if (!visualEntry.borderRadius.isEmpty()) {
                auto path = visualEntry.borderRadius.getPath(visualEntry.bounds);
                drawingContext.drawPaint(resolvedPaint, path);
                continue;
            }
        } else if (_gradientWrapper.hasGradient()) {
            applyGradientToTextPaint(resolvedPaint);
        } else if (visualEntry.color) {
            resolvedPaint.setColor(visualEntry.color.value());
        }

        drawTextDecoration(
            drawingContext, resolvedPaint, visualEntry.style, visualEntry.customUnderlineStyle, visualEntry.bounds);
    }
}

void TextLayer::drawText(DrawingContext& drawingContext,
                         const sk_sp<SkTextBlob>& textBlob,
                         std::optional<Color> textColor) {
    auto resolvedPaint = _textPaint;
    if (_gradientWrapper.hasGradient()) {
        applyGradientToTextPaint(resolvedPaint);
    } else {
        if (textColor) {
            resolvedPaint.setColor(textColor.value());
        }
    }

    drawingContext.canvas()->drawTextBlob(textBlob, 0, 0, resolvedPaint.getSkValue());
}

void TextLayer::applyGradientToTextPaint(Paint& paint) {
    if (_textLayout != nullptr) {
        _gradientWrapper.update(_textLayout->getBounds());
        _gradientWrapper.applyToPaint(paint);
    }
}

bool TextLayer::hasTextShadow() const {
    auto isVisible = _textShadow.opacity > 0;
    auto hiddenBehindText = _textShadow.radius == 0 && _textShadow.offsetX == 0 && _textShadow.offsetY == 0;
    return isVisible && !hiddenBehindText;
}

void TextLayer::drawTextShadows(DrawingContext& drawingContext, const sk_sp<SkTextBlob>& textBlob) {
    auto resolvedPaint = _textPaint;

    resolvedPaint.getSkValue().setMaskFilter(
        SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle, _textShadow.radius, false));
    resolvedPaint.setColor(_textShadow.color);
    resolvedPaint.setAlpha(_textShadow.opacity);

    drawingContext.canvas()->drawTextBlob(
        textBlob.get(), _textShadow.offsetX, _textShadow.offsetY, resolvedPaint.getSkValue());
}

void TextLayer::setText(const Valdi::StringBox& text) {
    if (_text != text || _attributedText != nullptr) {
        _text = text;
        _attributedText = nullptr;

        setNeedsTextLayout();
    }
}

const Valdi::StringBox& TextLayer::getText() const {
    return _text;
}

void TextLayer::setAttributedText(const Valdi::Ref<AttributedText>& attributedText) {
    if (_attributedText != attributedText) {
        _attributedText = attributedText;
        _text = Valdi::StringBox();

        setNeedsTextLayout();
    }
}

const Valdi::Ref<AttributedText>& TextLayer::getAttributedText() const {
    return _attributedText;
}

void TextLayer::setTextColor(Color textColor) {
    if (_textPaint.getColor() != textColor) {
        _textPaint.setColor(textColor);

        setNeedsDisplay();
    }
}

Color TextLayer::getTextColor() const {
    return _textPaint.getColor();
}

void TextLayer::setTextLinearGradient(std::vector<Scalar>&& locations,
                                      std::vector<Color>&& colors,
                                      LinearGradientOrientation orientation) {
    if (_gradientWrapper.clearIfNeeded(GradientWrapper::GradientType::RADIAL)) {
        setNeedsDisplay();
    }

    if (colors.empty()) {
        if (_gradientWrapper.clearIfNeeded(GradientWrapper::GradientType::LINEAR)) {
            setNeedsDisplay();
        }
        return;
    }
    _gradientWrapper.setAsLinear(std::move(locations), std::move(colors), orientation);

    if (_gradientWrapper.isDirty()) {
        setNeedsDisplay();
    }
}

void TextLayer::setTextRadialGradient(std::vector<Scalar>&& locations, std::vector<Color>&& colors) {
    if (_gradientWrapper.clearIfNeeded(GradientWrapper::GradientType::LINEAR)) {
        setNeedsDisplay();
    }

    if (colors.empty()) {
        if (_gradientWrapper.clearIfNeeded(GradientWrapper::GradientType::RADIAL)) {
            setNeedsDisplay();
        }
        return;
    }

    _gradientWrapper.setAsRadial(std::move(locations), std::move(colors));

    if (_gradientWrapper.isDirty()) {
        setNeedsDisplay();
    }
}

void TextLayer::resetTextGradient() {
    _gradientWrapper.clear();
    setNeedsDisplay();
}

void TextLayer::setTextAlign(TextAlign textAlign) {
    if (_textAlign != textAlign) {
        _textAlign = textAlign;

        setNeedsTextLayout();
    }
}

TextAlign TextLayer::getTextAlign() const {
    return _textAlign;
}

void TextLayer::setTextVerticalAlignment(TextVerticalAlignment textVerticalAlignment) {
    if (_textVerticalAlignment != textVerticalAlignment) {
        _textVerticalAlignment = textVerticalAlignment;
        setNeedsDisplay();
    }
}

TextVerticalAlignment TextLayer::getTextVerticalAlignment() const {
    return _textVerticalAlignment;
}

Scalar TextLayer::getTextVerticalOffset(const Size& layerSize, const TextLayout& layout, Scalar displayScale) const {
    if (_textVerticalAlignment == TextVerticalAlignmentCenter) {
        auto layoutHeight = layout.getBounds().height() / displayScale;
        return std::max<Scalar>((layerSize.height - layoutHeight) / 2.0f, 0.0f);
    }

    return 0.0f;
}

void TextLayer::setTextDecoration(TextDecoration textDecoration) {
    if (_textDecoration != textDecoration) {
        _textDecoration = textDecoration;
        setNeedsTextLayout();
    }
}

TextDecoration TextLayer::getTextDecoration() const {
    return _textDecoration;
}

void TextLayer::setCustomUnderlineStyle(std::optional<TextCustomUnderlineStyle> customUnderlineStyle) {
    if (_customUnderlineStyle != customUnderlineStyle) {
        _customUnderlineStyle = customUnderlineStyle;
        setNeedsTextLayout();
    }
}

const std::optional<TextCustomUnderlineStyle>& TextLayer::getCustomUnderlineStyle() const {
    return _customUnderlineStyle;
}

void TextLayer::setTextOverflow(TextOverflow textOveflow) {
    if (_textOverflow != textOveflow) {
        _textOverflow = textOveflow;
        setNeedsTextLayout();
    }
}

TextOverflow TextLayer::getTextOverflow() const {
    return _textOverflow;
}

void TextLayer::setTextShadow(Color color, Scalar radius, float opacity, Scalar offsetX, Scalar offsetY) {
    auto textShadow = TextShadow(color, radius, opacity, offsetX, offsetY);
    if (_textShadow != textShadow) {
        _textShadow = textShadow;
        setNeedsDisplay();
    }
}

void TextLayer::resetTextShadow() {
    setTextShadow(Color(), 0, 0, 0, 0);
}

TextShadow TextLayer::getTextShadow() const {
    return _textShadow;
}

void TextLayer::setTextFont(const Valdi::Ref<Font>& font) {
    if (_textFont != font) {
        _textFont = font;

        setNeedsTextLayout();
    }
}

const Valdi::Ref<Font>& TextLayer::getTextFont() const {
    return _textFont;
}

void TextLayer::setNumberOfLines(int numberOfLines) {
    if (_numberOfLines != numberOfLines) {
        _numberOfLines = numberOfLines;

        setNeedsTextLayout();
    }
}

int TextLayer::getNumberOfLines() const {
    return _numberOfLines;
}

void TextLayer::setAdjustsFontSizeToFitWidth(bool adjustsFontSizeToFitWidth) {
    if (_adjustsFontSizeToFitWidth != adjustsFontSizeToFitWidth) {
        _adjustsFontSizeToFitWidth = adjustsFontSizeToFitWidth;
        setNeedsTextLayout();
    }
}

bool TextLayer::getAdjustsFontSizeToFitWidth() const {
    return _adjustsFontSizeToFitWidth;
}

void TextLayer::setMinimumScaleFactor(double minimumScaleFactor) {
    minimumScaleFactor = std::clamp(minimumScaleFactor, 0.0, 1.0);

    if (_minimumScaleFactor != minimumScaleFactor) {
        _minimumScaleFactor = minimumScaleFactor;
        if (_adjustsFontSizeToFitWidth) {
            setNeedsTextLayout();
        }
    }
}

double TextLayer::getMinimumScaleFactor() const {
    return _minimumScaleFactor;
}

void TextLayer::setLineHeight(Scalar lineHeight) {
    if (_usesExplicitLineHeight != true || _lineHeight != lineHeight) {
        _usesExplicitLineHeight = true;
        _lineHeight = lineHeight;
        setNeedsTextLayout();
    }
}

void TextLayer::resetLineHeight() {
    if (_usesExplicitLineHeight) {
        _usesExplicitLineHeight = false;
        _lineHeight = 0.0f;
        setNeedsTextLayout();
    }
}

void TextLayer::setLineHeightMultiple(Scalar lineHeightMultiple) {
    if (_lineHeightMultipleValue != lineHeightMultiple) {
        _lineHeightMultipleValue = lineHeightMultiple;
        setNeedsTextLayout();
    }
}

void TextLayer::setLetterSpacing(Scalar letterSpacing) {
    if (_letterSpacing != letterSpacing) {
        _letterSpacing = letterSpacing;
        setNeedsTextLayout();
    }
}

Scalar TextLayer::getLetterSpacing() const {
    return _letterSpacing;
}

Scalar TextLayer::getLineHeightMultiple() const {
    return _lineHeightMultipleValue;
}

void TextLayer::setNeedsTextLayout() {
    if (_textLayout != nullptr) {
        _textLayout = nullptr;
        if (_attributedTextOnTapGestureRecognizer != nullptr) {
            _attributedTextOnTapGestureRecognizer->setTextLayout(nullptr);
        }
        setNeedsDisplay();
    }
}

static bool hasOnTapAttributeInTextLayout(const TextLayout& textLayout) {
    for (const auto& attachment : textLayout.getAttachments()) {
        if (Valdi::castOrNull<AttributedTextOnTapAttribute>(attachment.attachment) != nullptr) {
            return true;
        }
    }

    return false;
}

TextLayout& TextLayer::getTextLayout(Size size, bool respectDynamicType, Scalar displayScale, Scalar dynamicTypeScale) {
    // Align maxSize to pixel grid
    auto maxSize = Size::make(ceilf(size.width * displayScale), ceilf(size.height * displayScale));

    if (_textLayout != nullptr && (_textLayout->getMaxSize() != maxSize)) {
        _textLayout = nullptr;
    }

    if (_textLayout == nullptr) {
        VALDI_TRACE("SnapDrawing.makeTextLayout");
        auto resolvedLineHeight = resolveLineHeight(displayScale);
        _textLayout = TextLayer::makeTextLayout(maxSize,
                                                _text,
                                                _attributedText,
                                                _textFont,
                                                _textAlign,
                                                _textDecoration,
                                                _textOverflow,
                                                _numberOfLines,
                                                resolvedLineHeight,
                                                _letterSpacing,
                                                isRightToLeft(),
                                                _adjustsFontSizeToFitWidth,
                                                _minimumScaleFactor,
                                                respectDynamicType,
                                                /* includeTextBlob*/ true,
                                                displayScale,
                                                dynamicTypeScale,
                                                getResources()->getFontManager(),
                                                _customUnderlineStyle);

        if (hasOnTapAttributeInTextLayout(*_textLayout)) {
            addOnTapGestureRecognizer();
        } else {
            removeOnTapGestureRecognizer();
        }
    }

    return *_textLayout;
}

TextLayoutLineHeight TextLayer::resolveLineHeight(Scalar displayScale) const {
    if (!_usesExplicitLineHeight) {
        return TextLayoutLineHeight::multiple(_lineHeightMultipleValue);
    }

    return TextLayoutLineHeight::absolute(_lineHeight * displayScale);
}

void TextLayer::removeOnTapGestureRecognizer() {
    if (_attributedTextOnTapGestureRecognizer != nullptr) {
        removeGestureRecognizer(_attributedTextOnTapGestureRecognizer);
        _attributedTextOnTapGestureRecognizer = nullptr;
    }
}

void TextLayer::addOnTapGestureRecognizer() {
    if (_attributedTextOnTapGestureRecognizer == nullptr) {
        _attributedTextOnTapGestureRecognizer =
            Valdi::makeShared<AttributedTextOnTapGestureRecognizer>(getResources()->getGesturesConfiguration());
        addGestureRecognizer(_attributedTextOnTapGestureRecognizer);
    }

    _attributedTextOnTapGestureRecognizer->setTextLayout(_textLayout);
}

void TextLayer::onRightToLeftChanged() {
    Layer::onRightToLeftChanged();

    setNeedsTextLayout();
}

Size TextLayer::measureText(Size maxSize,
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
                            const Ref<FontManager>& fontManager,
                            std::optional<TextCustomUnderlineStyle> customUnderlineStyle) {
    auto textLayout = makeTextLayout(maxSize,
                                     text,
                                     attributedText,
                                     font,
                                     textAlign,
                                     textDecoration,
                                     textOverflow,
                                     numberOfLines,
                                     lineHeight,
                                     letterSpacing,
                                     isRightToLeft,
                                     adjustsFontSizeToFitWidth,
                                     minimumScaleFactor,
                                     respectDynamicType,
                                     /* includeTextBlob*/ false,
                                     displayScale,
                                     dynamicTypeScale,
                                     fontManager,
                                     customUnderlineStyle);

    return textLayout->getBounds().size();
}

Ref<TextLayout> TextLayer::makeTextLayout(Size maxSize,
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
                                          const Ref<FontManager>& fontManager,
                                          std::optional<TextCustomUnderlineStyle> customUnderlineStyle) {
    if (!adjustsFontSizeToFitWidth || numberOfLines != 1) {
        return makeTextLayoutUnscaled(maxSize,
                                      text,
                                      attributedText,
                                      font,
                                      textAlign,
                                      textDecoration,
                                      textOverflow,
                                      numberOfLines,
                                      lineHeight,
                                      letterSpacing,
                                      isRightToLeft,
                                      1.0,
                                      respectDynamicType,
                                      includeTextBlob,
                                      displayScale,
                                      dynamicTypeScale,
                                      fontManager,
                                      customUnderlineStyle);
    }

    auto currentScale = 1.0;
    auto scaleIncrement = (1.0 - minimumScaleFactor) / kMaxAdjustsFontSizeToFitWidthAttempt;

    for (;;) {
        auto layout = makeTextLayoutUnscaled(maxSize,
                                             text,
                                             attributedText,
                                             font,
                                             textAlign,
                                             textDecoration,
                                             textOverflow,
                                             numberOfLines,
                                             lineHeight,
                                             letterSpacing,
                                             isRightToLeft,
                                             currentScale,
                                             respectDynamicType,
                                             includeTextBlob,
                                             displayScale,
                                             dynamicTypeScale,
                                             fontManager,
                                             customUnderlineStyle);

        if (layout->fitsInMaxSize() || currentScale <= minimumScaleFactor) {
            return layout;
        } else {
            currentScale = std::max(minimumScaleFactor, currentScale - scaleIncrement);
        }
    }
}

static double resolveFontScale(
    const Ref<Font>& font, double fontScale, bool respectDynamicType, Scalar displayScale, Scalar dynamicTypeScale) {
    auto fontScaleRespectingDisplayScale = fontScale * displayScale;
    if (!respectDynamicType || !font->respectDynamicType()) {
        return fontScaleRespectingDisplayScale;
    }

    return fontScaleRespectingDisplayScale * dynamicTypeScale;
}

Ref<TextLayout> TextLayer::makeTextLayoutUnscaled(Size maxSize,
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
                                                  const Ref<FontManager>& fontManager,
                                                  std::optional<TextCustomUnderlineStyle> customUnderlineStyle) {
    TextLayoutBuilder builder(
        textAlign, textOverflow, maxSize, numberOfLines, fontManager, isRightToLeft, displayScale, false);
    builder.setIncludeTextBlob(includeTextBlob);

    auto textFont = font;
    if (textFont == nullptr && fontManager != nullptr) {
        auto defaultFont = fontManager->getDefaultFont();
        if (defaultFont) {
            textFont = defaultFont.moveValue();
        }
    }

    if (attributedText != nullptr) {
        for (size_t i = 0; i < attributedText->getPartsSize(); i++) {
            const auto& style = attributedText->getStyleAtIndex(i);
            const auto& content = attributedText->getContentAtIndex(i);

            Ref<Font> resolvedFont = style.font != nullptr ? style.font : textFont;
            if (resolvedFont == nullptr) {
                continue;
            }
            auto resolvedFontScale =
                resolveFontScale(resolvedFont, fontScale, respectDynamicType, displayScale, dynamicTypeScale);
            auto resolvedTextDecoration = style.textDecoration ? style.textDecoration.value() : textDecoration;
            std::optional<TextBackgroundStyle> backgroundStyle;
            if (style.backgroundColor) {
                backgroundStyle = TextBackgroundStyle{style.backgroundColor,
                                                      style.backgroundPadding.value_or(TextBackgroundPadding()),
                                                      style.backgroundBorderRadius.value_or(BorderRadius())};
            }

            builder.append(content.toStringView(),
                           resolvedFont->withScale(resolvedFontScale),
                           lineHeight,
                           letterSpacing,
                           resolvedTextDecoration,
                           style.onTap,
                           style.color,
                           backgroundStyle,
                           customUnderlineStyle);
        }
    } else if (!text.isEmpty() && textFont != nullptr) {
        auto resolvedFontScale =
            resolveFontScale(textFont, fontScale, respectDynamicType, displayScale, dynamicTypeScale);
        builder.append(text.toStringView(),
                       textFont->withScale(resolvedFontScale),
                       lineHeight,
                       letterSpacing,
                       textDecoration,
                       nullptr,
                       std::nullopt,
                       std::nullopt,
                       customUnderlineStyle);
    }

    return builder.build();
}

} // namespace snap::drawing
