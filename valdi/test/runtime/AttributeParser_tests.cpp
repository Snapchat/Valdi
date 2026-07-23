//
//  AttributeParser_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 23/01/23
//

#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

TEST(AttributeParser, canParseColor) {
    ColorPalette colorPalette(STRING_LITERAL("test"));

    AttributeParser parser("red rgba(0, 255, 0, 1.0) #0000ff #0000ff10 #f5a rgba(50, 25, 100, 0.5) rgba(25%, 10%, 5%, "
                           "0.1) rgba(255, 255, 255, 5%)");

    auto c = parser.parseColor(colorPalette);

    ASSERT_TRUE(c);
    ASSERT_EQ(Color::rgba(255, 0, 0, 1.0), c.value());
    ASSERT_EQ(static_cast<size_t>(3), parser.position());
    ASSERT_FALSE(parser.hasError());
    ASSERT_FALSE(parser.isAtEnd());

    c = parser.parseColor(colorPalette);

    ASSERT_TRUE(c);
    ASSERT_EQ(Color::rgba(0, 255, 0, 1.0), c.value());
    ASSERT_EQ(static_cast<size_t>(24), parser.position());
    ASSERT_FALSE(parser.hasError());
    ASSERT_FALSE(parser.isAtEnd());

    c = parser.parseColor(colorPalette);

    ASSERT_TRUE(c);
    ASSERT_EQ(Color::rgba(0, 0, 255, 1.0), c.value());
    ASSERT_EQ(static_cast<size_t>(32), parser.position());
    ASSERT_FALSE(parser.hasError());
    ASSERT_FALSE(parser.isAtEnd());

    c = parser.parseColor(colorPalette);

    ASSERT_TRUE(c);
    ASSERT_EQ(Color::rgba(0, 0, 255, 16 / 255.0), c.value());
    ASSERT_EQ(static_cast<size_t>(42), parser.position());
    ASSERT_FALSE(parser.hasError());
    ASSERT_FALSE(parser.isAtEnd());

    c = parser.parseColor(colorPalette);

    ASSERT_TRUE(c);
    ASSERT_EQ(Color::rgba(255, 85, 170, 1.0), c.value());
    ASSERT_EQ(static_cast<size_t>(47), parser.position());
    ASSERT_FALSE(parser.hasError());
    ASSERT_FALSE(parser.isAtEnd());

    c = parser.parseColor(colorPalette);

    ASSERT_TRUE(c);
    ASSERT_EQ(Color::rgba(50, 25, 100, 0.5), c.value());
    ASSERT_EQ(static_cast<size_t>(70), parser.position());
    ASSERT_FALSE(parser.hasError());
    ASSERT_FALSE(parser.isAtEnd());

    c = parser.parseColor(colorPalette);

    ASSERT_TRUE(c);
    ASSERT_EQ(Color::rgba(63, 25, 12, 25 / 255.0), c.value());
    ASSERT_EQ(static_cast<size_t>(94), parser.position());
    ASSERT_FALSE(parser.hasError());
    ASSERT_FALSE(parser.isAtEnd());

    c = parser.parseColor(colorPalette);

    ASSERT_TRUE(c);
    ASSERT_EQ(Color::rgba(255, 255, 255, 12 / 255.0), c.value());
    ASSERT_EQ(static_cast<size_t>(118), parser.position());
    ASSERT_FALSE(parser.hasError());
    ASSERT_TRUE(parser.isAtEnd());
}

TEST(AttributeParser, failsToParseColorOnColorPaletteMismatch) {
    ColorPalette colorPalette(STRING_LITERAL("test"));

    AttributeParser parser("reddish");

    auto c = parser.parseColor(colorPalette);

    ASSERT_FALSE(c);
    ASSERT_TRUE(parser.hasError());
    ASSERT_TRUE(parser.isAtEnd());
}

TEST(AttributeParser, canParseAngle) {
    AttributeParser parser("40deg 3rad");

    auto a = parser.parseAngle();

    ASSERT_TRUE(a);
    ASSERT_EQ(40 * M_PI / 180.0, a.value());
    ASSERT_EQ(static_cast<size_t>(5), parser.position());
    ASSERT_FALSE(parser.isAtEnd());

    a = parser.parseAngle();

    ASSERT_TRUE(a);
    ASSERT_EQ(3.0, a.value());
    ASSERT_EQ(static_cast<size_t>(10), parser.position());
    ASSERT_TRUE(parser.isAtEnd());
}

TEST(AttributeParser, failsToParseAnglWhenUnitIsMissing) {
    AttributeParser parser("40");

    auto a = parser.parseAngle();

    ASSERT_FALSE(a);
    ASSERT_TRUE(parser.hasError());
    ASSERT_TRUE(parser.isAtEnd());
}

TEST(AttributeParser, canParseDimension) {
    AttributeParser parser("40 42pt 95px 57%");

    auto d = parser.parseDimension();

    ASSERT_TRUE(d);
    ASSERT_EQ(Dimension(40.0, Dimension::Unit::None), d.value());
    ASSERT_EQ(static_cast<size_t>(2), parser.position());
    ASSERT_FALSE(parser.isAtEnd());

    d = parser.parseDimension();

    ASSERT_TRUE(d);
    ASSERT_EQ(Dimension(42.0, Dimension::Unit::Pt), d.value());
    ASSERT_EQ(static_cast<size_t>(7), parser.position());
    ASSERT_FALSE(parser.isAtEnd());

    d = parser.parseDimension();

    ASSERT_TRUE(d);
    ASSERT_EQ(Dimension(95.0, Dimension::Unit::Px), d.value());
    ASSERT_EQ(static_cast<size_t>(12), parser.position());
    ASSERT_FALSE(parser.isAtEnd());

    d = parser.parseDimension();

    ASSERT_TRUE(d);
    ASSERT_EQ(Dimension(57.0, Dimension::Unit::Percent), d.value());
    ASSERT_EQ(static_cast<size_t>(16), parser.position());
    ASSERT_TRUE(parser.isAtEnd());
}

} // namespace ValdiTest
