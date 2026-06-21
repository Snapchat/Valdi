//
//  TextLayout.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/14/20.
//

#pragma once

#include "snap_drawing/cpp/Text/Font.hpp"
#include "snap_drawing/cpp/Utils/Aliases.hpp"
#include "snap_drawing/cpp/Utils/BorderRadius.hpp"
#include "snap_drawing/cpp/Utils/Color.hpp"

#include "snap_drawing/cpp/Utils/Geometry.hpp"

#include "include/core/SkTextBlob.h"

#include "valdi_core/cpp/Utils/Value.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

namespace snap::drawing {

enum TextAlign {
    TextAlignLeft,
    TextAlignRight,
    TextAlignCenter,
    TextAlignJustify,
};

enum TextDecoration {
    TextDecorationNone,
    TextDecorationStrikethrough,
    TextDecorationUnderline,
    TextDecorationDashedUnderline,
    TextDecorationDottedUnderline,
};

enum TextOverflow { TextOverflowEllipsis, TextOverflowClip };

struct LineMetrics {
    Scalar ascent = 0;
    Scalar descent = 0;

    constexpr LineMetrics() = default;
    constexpr LineMetrics(Scalar ascent, Scalar descent) : ascent(ascent), descent(descent) {}

    constexpr Scalar height() const {
        return descent - ascent;
    }

    void join(const LineMetrics& other) {
        ascent = std::min(ascent, other.ascent);
        descent = std::max(descent, other.descent);
    }
};

class TextLayoutLineHeight {
    enum class Kind { Multiple, Absolute };

public:
    constexpr TextLayoutLineHeight() = default;

    static constexpr TextLayoutLineHeight multiple(Scalar multiple) {
        return TextLayoutLineHeight(Kind::Multiple, multiple);
    }

    static constexpr TextLayoutLineHeight absolute(Scalar height) {
        return TextLayoutLineHeight(Kind::Absolute, height);
    }

    LineMetrics getLineMetrics(const FontMetrics& fontMetrics) const;

private:
    Kind _kind = Kind::Multiple;
    Scalar _value = 1.0f;

    constexpr TextLayoutLineHeight(Kind kind, Scalar value) : _kind(kind), _value(value) {}
};

enum TextLayoutDecorationStyle {
    TextLayoutDecorationStyleSolid,
    TextLayoutDecorationStyleDashed,
    TextLayoutDecorationStyleDotted,
};

enum TextLayoutVisualEntryKind {
    TextLayoutVisualEntryKindBackground,
    TextLayoutVisualEntryKindDecoration,
};

struct TextBackgroundPadding {
    Scalar left = 0;
    Scalar top = 0;
    Scalar right = 0;
    Scalar bottom = 0;

    constexpr bool operator==(const TextBackgroundPadding& other) const {
        return left == other.left && top == other.top && right == other.right && bottom == other.bottom;
    }

    constexpr bool operator!=(const TextBackgroundPadding& other) const {
        return !(*this == other);
    }
};

struct TextLayoutVisualEntry {
    Rect bounds;
    /**
     Whether the entry should be drawn before or after the text blobs
     */
    bool predraw;
    std::optional<Color> color;
    TextLayoutVisualEntryKind kind = TextLayoutVisualEntryKindDecoration;
    TextLayoutDecorationStyle style = TextLayoutDecorationStyleSolid;
    BorderRadius borderRadius;

    TextLayoutVisualEntry() = default;
    TextLayoutVisualEntry(const Rect& bounds,
                          bool predraw,
                          std::optional<Color> color,
                          TextLayoutVisualEntryKind kind,
                          TextLayoutDecorationStyle style,
                          BorderRadius borderRadius = BorderRadius())
        : bounds(bounds), predraw(predraw), color(color), kind(kind), style(style), borderRadius(borderRadius) {}

    TextLayoutVisualEntry(const Rect& bounds,
                          bool predraw,
                          std::optional<Color> color,
                          TextLayoutDecorationStyle style)
        : TextLayoutVisualEntry(bounds, predraw, color, TextLayoutVisualEntryKindDecoration, style) {}
};

struct TextLayoutEntrySegment {
    Rect bounds;
    Ref<Font> font;
    std::string characters;

    TextLayoutEntrySegment();
    TextLayoutEntrySegment(const Rect& bounds, const Ref<Font>& font, const std::string& characters);
};

struct TextLayoutEntry {
    sk_sp<SkTextBlob> textBlob;
    Rect bounds;
    std::optional<Color> color;
    std::vector<TextLayoutEntrySegment> segments;

    TextLayoutEntry();
    TextLayoutEntry(const Rect& bounds, std::optional<Color> color, std::vector<TextLayoutEntrySegment> segments);

    ~TextLayoutEntry();
};

struct TextLayoutAttachment {
    Rect bounds;
    Ref<Valdi::RefCountable> attachment;

    inline TextLayoutAttachment(const Rect& bounds, const Ref<Valdi::RefCountable>& attachment)
        : bounds(bounds), attachment(attachment) {}
};

class TextLayout : public Valdi::SimpleRefCountable {
public:
    TextLayout(Size maxSize,
               std::vector<TextLayoutEntry>&& entries,
               std::vector<TextLayoutVisualEntry>&& visualEntries,
               std::vector<TextLayoutAttachment>&& attachments,
               bool fitsInMaxSize);
    ~TextLayout() override;

    const std::vector<TextLayoutEntry>& getEntries() const;
    const std::vector<TextLayoutVisualEntry>& getVisualEntries() const;
    const std::vector<TextLayoutAttachment>& getAttachments() const;

    const Size& getMaxSize() const;

    const Rect& getBounds() const;

    bool fitsInMaxSize() const;

    Valdi::Value toJSONValue() const;

    Ref<Valdi::RefCountable> getAttachmentAtPoint(Point location, Scalar tolerance) const;

private:
    Size _maxSize;
    bool _fitsInMaxSize;

    std::vector<TextLayoutEntry> _entries;
    std::vector<TextLayoutVisualEntry> _visualEntries;
    std::vector<TextLayoutAttachment> _attachments;
    Rect _bounds = Rect::makeEmpty();
};

} // namespace snap::drawing
