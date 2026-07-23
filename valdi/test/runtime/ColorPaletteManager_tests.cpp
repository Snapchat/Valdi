#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "gtest/gtest.h"

using namespace Valdi;

namespace ValdiTest {

class TestColorPaletteManagerListener : public ColorPaletteManagerListener {
public:
    void onColorPaletteManagerUpdated(const ColorPaletteManager& colorPaletteManager,
                                      const ColorPalette& colorPalette,
                                      bool activeColorPaletteChanged) override {
        updateCount++;
        lastActiveColorPaletteName = colorPaletteManager.getActiveColorPalette()->getName();
        lastUpdatedColorPaletteName = colorPalette.getName();
        lastActiveColorPaletteChanged = activeColorPaletteChanged;
    }

    int updateCount = 0;
    StringBox lastActiveColorPaletteName;
    StringBox lastUpdatedColorPaletteName;
    bool lastActiveColorPaletteChanged = false;
};

TEST(ColorPaletteManager, defaultsToDefaultPaletteWithCssColors) {
    ColorPaletteManager manager;

    ASSERT_EQ(STRING_LITERAL("default"), manager.getActiveColorPalette()->getName());
    ASSERT_EQ(Color::rgba(255, 0, 0, 1.0),
              manager.getActiveColorPalette()->getColorForName(STRING_LITERAL("red")).value());
}

TEST(ColorPaletteManager, notifiesWhenConfiguringInactivePaletteWithChangedValuesOnly) {
    ColorPaletteManager manager;
    TestColorPaletteManagerListener listener;
    manager.setListener(&listener);

    manager.configureColorPalette(STRING_LITERAL("dark"), {{STRING_LITERAL("background"), Color::rgba(0, 0, 0, 1.0)}});
    manager.configureColorPalette(STRING_LITERAL("dark"), {{STRING_LITERAL("background"), Color::rgba(0, 0, 0, 1.0)}});

    ASSERT_EQ(1, listener.updateCount);
    ASSERT_EQ(STRING_LITERAL("default"), manager.getActiveColorPalette()->getName());
    ASSERT_EQ(STRING_LITERAL("dark"), listener.lastUpdatedColorPaletteName);
    ASSERT_FALSE(listener.lastActiveColorPaletteChanged);
}

TEST(ColorPaletteManager, notifiesWhenConfiguringActivePaletteWithChangedValuesOnly) {
    ColorPaletteManager manager;
    TestColorPaletteManagerListener listener;
    manager.setListener(&listener);

    manager.configureColorPalette(STRING_LITERAL("default"),
                                  {{STRING_LITERAL("background"), Color::rgba(255, 255, 255, 1.0)}});
    manager.configureColorPalette(STRING_LITERAL("default"),
                                  {{STRING_LITERAL("background"), Color::rgba(255, 255, 255, 1.0)}});

    ASSERT_EQ(1, listener.updateCount);
    ASSERT_EQ(STRING_LITERAL("default"), listener.lastUpdatedColorPaletteName);
    ASSERT_FALSE(listener.lastActiveColorPaletteChanged);
    ASSERT_EQ(Color::rgba(255, 255, 255, 1.0),
              manager.getActiveColorPalette()->getColorForName(STRING_LITERAL("background")).value());
}

TEST(ColorPaletteManager, notifiesWhenActivePaletteChangesOnly) {
    ColorPaletteManager manager;
    TestColorPaletteManagerListener listener;
    manager.setListener(&listener);

    manager.setActiveColorPalette(STRING_LITERAL("dark"));
    manager.setActiveColorPalette(STRING_LITERAL("dark"));

    ASSERT_EQ(1, listener.updateCount);
    ASSERT_EQ(STRING_LITERAL("dark"), listener.lastActiveColorPaletteName);
    ASSERT_EQ(STRING_LITERAL("dark"), listener.lastUpdatedColorPaletteName);
    ASSERT_TRUE(listener.lastActiveColorPaletteChanged);
}

TEST(ColorPaletteManager, createsDefaultInitializedPaletteWhenActivatingUnknownName) {
    ColorPaletteManager manager;

    manager.setActiveColorPalette(STRING_LITERAL("dark"));

    ASSERT_EQ(STRING_LITERAL("dark"), manager.getActiveColorPalette()->getName());
    ASSERT_EQ(Color::rgba(255, 0, 0, 1.0),
              manager.getActiveColorPalette()->getColorForName(STRING_LITERAL("red")).value());
}

} // namespace ValdiTest
