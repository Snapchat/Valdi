#include "DisplayListBuilder.hpp"
#include "snap_drawing/cpp/Drawing/Raster/RasterDamageResolver.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace snap::drawing {

class RasterDamageResolverTests : public ::testing::Test {
protected:
    DisplayListBuilder _builder = DisplayListBuilder(100, 100);
    RasterDamageResolver _damageResolver;

    void SetUp() override {
        _builder = DisplayListBuilder(100, 100);
        _damageResolver = RasterDamageResolver();
    }

    std::vector<Rect> resolveDamage() {
        _damageResolver.beginUpdates(100, 100);
        _damageResolver.addDamageFromDisplayListUpdates(*_builder.displayList);
        return _damageResolver.endUpdates();
    }
};

TEST_F(RasterDamageResolverTests, returnsFullRectOnInInitialDraw) {
    _builder.context(Vector(0, 0), 1.0, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
    });
    auto damageRects = resolveDamage();

    ASSERT_EQ(static_cast<size_t>(1), damageRects.size());
    // Damage rects include 1px margin for anti-aliasing
    ASSERT_EQ(Rect::makeXYWH(-1, -1, 102, 102), damageRects[0]);
}

TEST_F(RasterDamageResolverTests, returnsPartialDamageRect) {
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, true, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
    });

    // First pass to populate the previous layer contents
    resolveDamage();
    auto damageRects = resolveDamage();

    ASSERT_EQ(static_cast<size_t>(1), damageRects.size());
    // Damage rects include 1px margin for anti-aliasing: (50-1, 50-1, 10+2, 10+2)
    ASSERT_EQ(Rect::makeXYWH(49, 49, 12, 12), damageRects[0]);
}

TEST_F(RasterDamageResolverTests, returnsMultipleDamageRects) {
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, true, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
        _builder.context(Vector(20, 20), 1.0, 3, true, [&]() { _builder.rectangle(Size(15, 15), 1.0); });
    });

    // First pass to populate the previous layer contents
    resolveDamage();
    auto damageRects = resolveDamage();

    ASSERT_EQ(static_cast<size_t>(2), damageRects.size());
    // Damage rects include 1px margin for anti-aliasing
    ASSERT_EQ(Rect::makeXYWH(49, 49, 12, 12), damageRects[0]);
    ASSERT_EQ(Rect::makeXYWH(19, 19, 17, 17), damageRects[1]);
}

TEST_F(RasterDamageResolverTests, mergesMultipleDamageRectsWhenPossible) {
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, true, [&]() { _builder.rectangle(Size(20, 20), 1.0); });
        _builder.context(Vector(20, 20), 1.0, 3, true, [&]() { _builder.rectangle(Size(40, 40), 1.0); });
    });

    // First pass to populate the previous layer contents
    resolveDamage();
    auto damageRects = resolveDamage();

    ASSERT_EQ(static_cast<size_t>(1), damageRects.size());
    // Rects (50,50,20,20) and (20,20,40,40) with 1px margin merge into (19,19,52,52)
    ASSERT_EQ(Rect::makeXYWH(19, 19, 52, 52), damageRects[0]);
}

TEST_F(RasterDamageResolverTests, returnsEmptyDamageRectsWhenNoDamage) {
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, false, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
        _builder.context(Vector(20, 20), 1.0, 3, false, [&]() { _builder.rectangle(Size(50, 50), 1.0); });
    });

    // First pass to populate the previous layer contents
    resolveDamage();
    auto damageRects = resolveDamage();

    ASSERT_EQ(static_cast<size_t>(0), damageRects.size());
}

TEST_F(RasterDamageResolverTests, returnsDamageOnInsertedLayer) {
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, false, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
        _builder.context(Vector(20, 20), 1.0, 3, false, [&]() { _builder.rectangle(Size(50, 50), 1.0); });
    });

    // First pass to populate the previous layer contents
    resolveDamage();

    _builder = DisplayListBuilder(100, 100);
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, false, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
        _builder.context(Vector(20, 20), 1.0, 3, false, [&]() { _builder.rectangle(Size(50, 50), 1.0); });
        _builder.context(Vector(10, 10), 1.0, 4, true, [&]() { _builder.rectangle(Size(15, 15), 1.0); });
    });

    auto damageRects = resolveDamage();

    ASSERT_EQ(static_cast<size_t>(1), damageRects.size());
    // Damage rects include 1px margin for anti-aliasing: (10-1, 10-1, 15+2, 15+2)
    ASSERT_EQ(Rect::makeXYWH(9, 9, 17, 17), damageRects[0]);
}

TEST_F(RasterDamageResolverTests, returnsDamageOnRemovedLayer) {
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, false, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
        _builder.context(Vector(20, 20), 1.0, 3, false, [&]() { _builder.rectangle(Size(50, 50), 1.0); });
    });

    // First pass to populate the previous layer contents
    resolveDamage();

    _builder = DisplayListBuilder(100, 100);
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, false, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
    });

    auto damageRects = resolveDamage();

    ASSERT_EQ(static_cast<size_t>(1), damageRects.size());
    // Damage rects include 1px margin for anti-aliasing: (20-1, 20-1, 50+2, 50+2)
    ASSERT_EQ(Rect::makeXYWH(19, 19, 52, 52), damageRects[0]);
}

TEST_F(RasterDamageResolverTests, returnsDamageOnMovedLayer) {
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(50, 50), 1.0, 2, false, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
        _builder.context(Vector(20, 20), 1.0, 3, false, [&]() { _builder.rectangle(Size(50, 50), 1.0); });
    });

    // First pass to populate the previous layer contents
    resolveDamage();

    _builder = DisplayListBuilder(100, 100);
    _builder.context(Vector(0, 0), 1.0, 1, false, [&]() {
        _builder.rectangle(Size(100, 100), 1.0);
        _builder.context(Vector(10, 10), 1.0, 2, false, [&]() { _builder.rectangle(Size(10, 10), 1.0); });
        _builder.context(Vector(20, 20), 1.0, 3, false, [&]() { _builder.rectangle(Size(50, 50), 1.0); });
    });

    auto damageRects = resolveDamage();

    ASSERT_EQ(static_cast<size_t>(2), damageRects.size());
    // Damage rects include 1px margin for anti-aliasing
    ASSERT_EQ(Rect::makeXYWH(49, 49, 12, 12), damageRects[0]);
    ASSERT_EQ(Rect::makeXYWH(9, 9, 12, 12), damageRects[1]);
}

} // namespace snap::drawing