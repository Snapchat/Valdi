#include "valdi/terminal/Classes/ViewClass.hpp"

#include "valdi/terminal/Classes/TerminalAttributeMacros.hpp"
#include "valdi/terminal/Paint/Painter.hpp"

namespace Valdi {

ViewClass::ViewClass() : ITerminalViewClass("SCValdiView", "com.snap.valdi.views.ValdiView", nullptr, false) {}

ViewClass::~ViewClass() = default;

static std::optional<Terminal::TermColor> toTermColor(const Value& value) {
    return Terminal::TermColor::fromValdiColor(Color(value.toLong()));
}

static void bindSide(AttributesBindingContext& binder,
                     const char* flagName,
                     const char* colorName,
                     TerminalBorderSide side) {
    const auto index = static_cast<size_t>(side);
    binder.bindBooleanAttribute(StringCache::getGlobal().makeString(std::string_view(flagName)),
                                false,
                                TerminalAttributes::makeDelegate(
                                    [index](TerminalView& view, const Value& value) -> Result<Void> {
                                        view.style().sideEnabled[index] = value.toBool();
                                        return Void();
                                    },
                                    [index](TerminalView& view) { view.style().sideEnabled[index] = true; }));
    binder.bindColorAttribute(StringCache::getGlobal().makeString(std::string_view(colorName)),
                              false,
                              TerminalAttributes::makeDelegate(
                                  [index](TerminalView& view, const Value& value) -> Result<Void> {
                                      view.style().sideColors[index] = toTermColor(value);
                                      return Void();
                                  },
                                  [index](TerminalView& view) { view.style().sideColors[index].reset(); }));
}

void ViewClass::bindAttributes(AttributesBindingContext& binder) {
    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Color,
        "backgroundColor",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.style().backgroundColor = toTermColor(value);
            return Void();
        },
        [](TerminalView& view) { view.style().backgroundColor.reset(); });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Color,
        "borderColor",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.style().borderColor = toTermColor(value);
            return Void();
        },
        [](TerminalView& view) { view.style().borderColor.reset(); });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Double,
        "borderWidth",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.style().borderWidth = value.toDouble();
            return Void();
        },
        [](TerminalView& view) { view.style().borderWidth = 0; });

    // The attributes manager preprocesses "1 solid red" style strings into [width, color], as for SnapDrawing.
    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Untyped,
        "border",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            const auto* entries = value.getArray();
            if (entries == nullptr) {
                return Error("Missing border components");
            }
            view.style().borderWidth = entries->empty() ? 0.0 : (*entries)[0].toDouble();
            view.style().borderColor.reset();
            if (entries->size() > 1) {
                view.style().borderColor = toTermColor((*entries)[1]);
            }
            return Void();
        },
        [](TerminalView& view) {
            view.style().borderWidth = 0;
            view.style().borderColor.reset();
        });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        String,
        "borderStyle",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.style().borderStyle = value.toString();
            return Void();
        },
        [](TerminalView& view) { view.style().borderStyle = "single"; });

    TERMINAL_BIND_ATTRIBUTE(
        binder,
        Boolean,
        "borderDim",
        false,
        [](TerminalView& view, const Value& value) -> Result<Void> {
            view.style().borderDim = value.toBool();
            return Void();
        },
        [](TerminalView& view) { view.style().borderDim = false; });

    bindSide(binder, "borderTop", "borderTopColor", TerminalBorderSide::Top);
    bindSide(binder, "borderBottom", "borderBottomColor", TerminalBorderSide::Bottom);
    bindSide(binder, "borderLeft", "borderLeftColor", TerminalBorderSide::Left);
    bindSide(binder, "borderRight", "borderRightColor", TerminalBorderSide::Right);
}

void ViewClass::paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) {
    painter.paintBox(view, area);
    painter.paintChildren(view, area.rect, area.clip, view.clipsChildren());
}

} // namespace Valdi
