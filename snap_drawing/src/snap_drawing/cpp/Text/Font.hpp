//
//  Font.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 6/29/20.
//

#pragma once

#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"

#include "snap_drawing/cpp/Utils/Aliases.hpp"

#include "snap_drawing/cpp/Text/FontMetrics.hpp"
#include "snap_drawing/cpp/Text/FontStyle.hpp"
#include "snap_drawing/cpp/Text/Harfbuzz.hpp"
#include "snap_drawing/cpp/Utils/Geometry.hpp"

#include "include/core/SkFont.h"
#include "include/core/SkFontTypes.h"

namespace snap::drawing {

class Font;

class Typeface;

class FontResolver : public Valdi::ValdiObject {
public:
    virtual Valdi::Result<Ref<Font>> getFont(
        const Ref<Typeface>& typeface, FontStyle fontStyle, Scalar fontSize, double scale, bool respectDynamicType) = 0;
};

using FontId = uint64_t;

class Font : public Valdi::ValdiObject {
public:
    Font(Ref<FontResolver>&& fontResolver,
         Ref<Typeface>&& typeface,
         Scalar size,
         double scale,
         bool respectDynamicType,
         SkFontHinting hinting = SkFontHinting::kNormal);
    ~Font() override;

    const SkFont& getSkValue() const;
    const FontStyle& style() const;
    const Ref<Typeface>& typeface() const;

    FontId getFontId() const;

    Scalar resolvedSize() const;

    Scalar size() const;
    double scale() const;
    bool respectDynamicType() const;

    const FontMetrics& metrics();

    std::string getDescription() const;

    Ref<Font> withScale(double scale);
    Valdi::Result<Ref<Font>> withStyle(FontStyle style);
    Valdi::Result<Ref<Font>> withSize(Scalar size);
    Valdi::Result<Ref<Font>> withStyleAndSize(FontStyle style, Scalar size);

    const HBFont& getHBFont();

    VALDI_CLASS_HEADER(Font)

private:
    Ref<FontResolver> _fontResolver;
    Ref<Typeface> _typeface;
    Ref<Valdi::RefCountable> _textShaperRepr;
    HBFont _hbFont;
    SkFont _font;
    Scalar _size;
    double _scale;
    bool _respectDynamicType;
    FontMetrics _metrics;
    bool _loadedMetrics = false;
    bool _hasHBFont = false;
};

} // namespace snap::drawing
