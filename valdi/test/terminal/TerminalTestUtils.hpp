#pragma once

#include "valdi/terminal/Classes/LabelClass.hpp"
#include "valdi/terminal/Paint/CellGrid.hpp"
#include "valdi/terminal/Paint/Painter.hpp"
#include "valdi/terminal/TerminalView.hpp"
#include "valdi/terminal/TerminalViewManager.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <string>
#include <vector>

namespace ValdiTest {

/** Builds TerminalView trees by hand, bypassing the runtime. */
class TerminalViewBuilder {
public:
    Valdi::TerminalViewManager manager;

    Valdi::Ref<Valdi::TerminalView> make(const char* className, float x, float y, float width, float height) {
        auto viewClass = manager.getViewClass(Valdi::StringCache::getGlobal().makeString(std::string_view(className)));
        auto view = viewClass->instantiate(viewClass, snap::valdi_core::Platform::Ios);
        view->setFrame(Valdi::Frame(x, y, width, height));
        return view;
    }

    Valdi::Ref<Valdi::TerminalView> box(float x, float y, float width, float height) {
        return make("SCValdiView", x, y, width, height);
    }

    Valdi::Ref<Valdi::TerminalView> text(
        float x, float y, float width, float height, const std::string& value, int numberOfLines = 0) {
        auto view = make("SCValdiLabel", x, y, width, height);
        view->label().params.runs = {Valdi::Terminal::TextRun{value, Valdi::Terminal::Style()}};
        view->label().params.numberOfLines = numberOfLines;
        return view;
    }
};

struct PaintedTree {
    Valdi::Terminal::CellGrid grid;
    std::vector<Valdi::TerminalView*> staticViews;
};

inline PaintedTree paintTree(Valdi::TerminalView& root, int width, int height) {
    Valdi::Terminal::CellGrid grid(width, height);
    Valdi::Terminal::Painter painter(grid);
    painter.paintRoot(root);
    auto staticViews = painter.staticViews();
    return PaintedTree{std::move(grid), std::move(staticViews)};
}

} // namespace ValdiTest
