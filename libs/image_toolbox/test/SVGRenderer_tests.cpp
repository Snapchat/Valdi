#include <cstring>
#include <gtest/gtest.h>

#include "image_toolbox/SVGRenderer.hpp"

namespace snap::imagetoolbox {

static const Valdi::Byte* bytesFromString(const char* value) {
    return reinterpret_cast<const Valdi::Byte*>(value);
}

TEST(SVGRenderer, getSizeReturnsIntrinsicDimensions) {
    const char* svg = "<svg width=\"24\" height=\"16\" xmlns=\"http://www.w3.org/2000/svg\"></svg>";

    auto result = SVGRenderer::getSize(bytesFromString(svg), strlen(svg));

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(24, result.value().first);
    EXPECT_EQ(16, result.value().second);
}

TEST(SVGRenderer, getSizeFailsOnMalformedSVG) {
    const char* svg = "<svg width=\"24\"";

    auto result = SVGRenderer::getSize(bytesFromString(svg), strlen(svg));

    ASSERT_FALSE(result);
}

TEST(SVGRenderer, getSizeFailsOnMissingClosingTag) {
    const char* svg = "<svg width=\"24\" height=\"16\" xmlns=\"http://www.w3.org/2000/svg\">"
                      "<rect width=\"24\" height=\"16\" fill=\"red\"/>";

    auto result = SVGRenderer::getSize(bytesFromString(svg), strlen(svg));

    ASSERT_FALSE(result);
}

TEST(SVGRenderer, renderProducesImageWithIntrinsicSize) {
    const char* svg = "<svg width=\"10\" height=\"8\" xmlns=\"http://www.w3.org/2000/svg\">"
                      "<rect width=\"10\" height=\"8\" fill=\"#ff0000\"/>"
                      "</svg>";

    auto result = SVGRenderer::render(bytesFromString(svg), strlen(svg), 0, 0);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(10, result.value()->width());
    EXPECT_EQ(8, result.value()->height());
}

TEST(SVGRenderer, renderScalesToRequestedSize) {
    const char* svg = "<svg width=\"10\" height=\"8\" xmlns=\"http://www.w3.org/2000/svg\">"
                      "<rect width=\"10\" height=\"8\" fill=\"#ff0000\"/>"
                      "</svg>";

    auto result = SVGRenderer::render(bytesFromString(svg), strlen(svg), 20, 16);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(20, result.value()->width());
    EXPECT_EQ(16, result.value()->height());
}

TEST(SVGRenderer, renderFailsOnMalformedSVG) {
    const char* svg = "<svg width=\"10\"";

    auto result = SVGRenderer::render(bytesFromString(svg), strlen(svg), 0, 0);

    ASSERT_FALSE(result);
}

TEST(SVGRenderer, renderFailsOnMissingClosingTag) {
    const char* svg = "<svg width=\"10\" height=\"8\" xmlns=\"http://www.w3.org/2000/svg\">"
                      "<rect width=\"10\" height=\"8\" fill=\"red\"/>";

    auto result = SVGRenderer::render(bytesFromString(svg), strlen(svg), 0, 0);

    ASSERT_FALSE(result);
}

} // namespace snap::imagetoolbox
