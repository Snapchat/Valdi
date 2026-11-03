#include "valdi/terminal/Classes/LabelClass.hpp"

#include "valdi/terminal/Classes/TerminalAttributeMacros.hpp"
#include "valdi/terminal/Paint/Painter.hpp"
#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace Valdi {

LabelClass::LabelClass(const Ref<ITerminalViewClass>& parentClass)
    : ITerminalViewClass("SCValdiLabel", "com.snap.valdi.views.ValdiTextView", parentClass, true) {}

LabelClass::~LabelClass() = default;

static bool contains(const std::string& text, std::string_view needle) {
    return text.find(needle) != std::string::npos;
}

Terminal::Style LabelClass::styleFromFontName(std::string_view fontName) {
    std::string name(fontName);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
    // The size follows the last space; only the family and weight part names a style.
    const auto sizeStart = name.find_last_of(' ');
    if (sizeStart != std::string::npos) {
        name.erase(sizeStart);
    }

    Terminal::Style style;
    if (contains(name, "bold") || contains(name, "heavy") || contains(name, "black")) {
        style.bold = true;
    } else if (contains(name, "light") || contains(name, "thin")) {
        style.dim = true;
    }
    if (contains(name, "italic") || contains(name, "oblique")) {
        style.italic = true;
    }
    return style;
}

static std::optional<Terminal::TermColor> toTermColor(const Value& value) {
    return Terminal::TermColor::fromValdiColor(Color(value.toLong()));
}

std::vector<Terminal::TextRun> LabelClass::parseRuns(const Value& value) {
    std::vector<Terminal::TextRun> runs;
    if (value.isString()) {
        runs.push_back(Terminal::TextRun{value.toString(), Terminal::Style()});
        return runs;
    }
    if (!value.isValdiObject()) {
        return runs;
    }
    auto attributed = castOrNull<TextAttributeValue>(value.getValdiObject());
    if (attributed == nullptr) {
        return runs;
    }
    for (size_t i = 0; i < attributed->getPartsSize(); i++) {
        const auto& source = attributed->getStyleAtIndex(i);
        Terminal::Style style;
        if (source.font) {
            style = styleFromFontName(source.font->toStringView());
        }
        if (source.color) {
            style.fg = Terminal::TermColor::fromValdiColor(*source.color);
        }
        if (source.textDecoration == TextDecoration::Underline) {
            style.underline = true;
        } else if (source.textDecoration == TextDecoration::Strikethrough) {
            style.strikethrough = true;
        }
        if (source.background != nullptr && source.background->color) {
            style.bg = Terminal::TermColor::fromValdiColor(*source.background->color);
        }
        runs.push_back(Terminal::TextRun{std::string(attributed->getContentAtIndex(i).toStringView()), style});
    }
    return runs;
}

Size LabelClass::onMeasure(const Ref<ValueMap>& attributes,
                           float width,
                           MeasureMode widthMode,
                           float /*height*/,
                           MeasureMode /*heightMode*/,
                           bool /*isRightToLeft*/) {
    static const auto kValue = STRING_LITERAL("value");
    static const auto kNumberOfLines = STRING_LITERAL("numberOfLines");
    static const auto kTextOverflow = STRING_LITERAL("textOverflow");
    static const auto kTextWrap = STRING_LITERAL("textWrap");

    const auto get = [&](const StringBox& key) -> const Value& {
        const auto it = attributes->find(key);
        return it == attributes->end() ? Value::undefinedRef() : it->second;
    };

    TerminalLabelParams params;
    params.runs = parseRuns(get(kValue));
    const auto& numberOfLines = get(kNumberOfLines);
    params.numberOfLines = numberOfLines.isNumber() ? numberOfLines.toInt() : 1;
    const auto& textOverflow = get(kTextOverflow);
    params.ellipsize = !(textOverflow.isString() && textOverflow.toStringBox() == "clip");
    const auto& textWrap = get(kTextWrap);
    if (textWrap.isString()) {
        params.wrap = Terminal::parseWrapMode(textWrap.toStringBox().toStringView());
    }

    std::optional<int> maxWidth;
    if (widthMode != MeasureModeUnspecified) {
        maxWidth = static_cast<int>(std::floor(std::max(0.0f, width)));
    }
    const auto metrics = Terminal::measureLines(params.layout(maxWidth));
    return Size(static_cast<float>(metrics.width), static_cast<float>(metrics.height));
}

void LabelClass::bindAttributes(AttributesBindingContext& binder) {
    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Text,
        "value",
        true,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.label().params.runs = parseRuns(value);
            view.label().invalidate();
            return Void();
        },
        [](TerminalView& view) {
            view.label().params.runs.clear();
            view.label().invalidate();
        });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Color,
        "color",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.label().baseStyle.fg = toTermColor(value);
            view.label().invalidate();
            return Void();
        },
        [](TerminalView& view) {
            view.label().baseStyle.fg.reset();
            view.label().invalidate();
        });

    // The font's size has no meaning in a terminal; only weight and slant survive as bold, dim and italic.
    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Untyped,
        "font",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            const auto fontStyle =
                value.isString() ? styleFromFontName(value.toStringBox().toStringView()) : Terminal::Style();
            auto& base = view.label().baseStyle;
            base.bold = fontStyle.bold;
            base.dim = fontStyle.dim;
            base.italic = fontStyle.italic;
            view.label().invalidate();
            return Void();
        },
        [](TerminalView& view) {
            auto& base = view.label().baseStyle;
            base.bold = false;
            base.dim = false;
            base.italic = false;
            view.label().invalidate();
        });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        String,
        "textDecoration",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            const auto decoration = value.toString();
            auto& base = view.label().baseStyle;
            base.underline = decoration == "underline";
            base.strikethrough = decoration == "strikethrough";
            view.label().invalidate();
            return Void();
        },
        [](TerminalView& view) {
            view.label().baseStyle.underline = false;
            view.label().baseStyle.strikethrough = false;
            view.label().invalidate();
        });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        String,
        "textAlign",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            const auto align = value.toString();
            view.label().align = align == "center" ? TerminalTextAlign::Center :
                                 align == "right"  ? TerminalTextAlign::Right :
                                                     TerminalTextAlign::Left;
            return Void();
        },
        [](TerminalView& view) { view.label().align = TerminalTextAlign::Left; });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Int,
        "numberOfLines",
        true,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.label().params.numberOfLines = value.toInt();
            view.label().invalidate();
            return Void();
        },
        [](TerminalView& view) {
            view.label().params.numberOfLines = 1;
            view.label().invalidate();
        });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        String,
        "textOverflow",
        true,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.label().params.ellipsize = value.toString() != "clip";
            view.label().invalidate();
            return Void();
        },
        [](TerminalView& view) {
            view.label().params.ellipsize = true;
            view.label().invalidate();
        });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Boolean,
        "inverse",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.label().baseStyle.inverse = value.toBool();
            view.label().invalidate();
            return Void();
        },
        [](TerminalView& view) {
            view.label().baseStyle.inverse = false;
            view.label().invalidate();
        });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        String,
        "textWrap",
        true,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.label().params.wrap = Terminal::parseWrapMode(value.toString());
            view.label().invalidate();
            return Void();
        },
        [](TerminalView& view) {
            view.label().params.wrap.reset();
            view.label().invalidate();
        });
}

void LabelClass::paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) {
    painter.paintBox(view, area);

    auto& label = view.label();
    if (label.params.runs.empty()) {
        return;
    }

    // Wrap at the same whole-cell width the measure callback used so lines match the measured height.
    const int wrapWidth = std::max(0, static_cast<int>(std::floor(view.getFrame().width + 0.5f)));
    const auto& lines = label.lines(wrapWidth);
    const auto& rect = area.rect;
    const auto region =
        Terminal::intersect(Terminal::IntRect(rect.x, rect.y, std::max(rect.width, wrapWidth), rect.height), area.clip);

    for (size_t i = 0; i < lines.size(); i++) {
        const int y = rect.y + static_cast<int>(i);
        if (y >= region.y + region.height) {
            break;
        }
        int offset = 0;
        if (label.align == TerminalTextAlign::Center) {
            offset = (rect.width - lines[i].width) / 2;
        } else if (label.align == TerminalTextAlign::Right) {
            offset = rect.width - lines[i].width;
        }
        painter.target().writeSegments(rect.x + std::max(0, offset), y, lines[i].segments, region);
    }
}

} // namespace Valdi
