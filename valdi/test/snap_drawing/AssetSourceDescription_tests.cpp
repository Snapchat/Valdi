#include "valdi/snap_drawing/ImageLoading/AssetSourceDescription.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace Valdi;
using namespace snap::drawing;

TEST(AssetSourceDescription, reportsHostAndExtension) {
    EXPECT_EQ("host=cf-st.sc-cdn.net ext=webp",
              describeAssetSource(StringBox::fromCString("https://cf-st.sc-cdn.net/media/sticker.webp")));
}

TEST(AssetSourceDescription, dropsQueryAndFragment) {
    EXPECT_EQ("host=cf-st.sc-cdn.net ext=webp",
              describeAssetSource(
                  StringBox::fromCString("https://cf-st.sc-cdn.net/media/sticker.webp?key=THEKEY&iv=THEIV#frag")));
    EXPECT_EQ("host=cf-st.sc-cdn.net ext=none",
              describeAssetSource(StringBox::fromCString("https://cf-st.sc-cdn.net?key=THEKEY")));
    EXPECT_EQ("host=cf-st.sc-cdn.net ext=none",
              describeAssetSource(StringBox::fromCString("https://cf-st.sc-cdn.net#THEKEY")));
}

TEST(AssetSourceDescription, ignoresADotInADirectoryName) {
    EXPECT_EQ("host=cf-st.sc-cdn.net ext=none",
              describeAssetSource(StringBox::fromCString("https://cf-st.sc-cdn.net/media/123.456/secret")));
}

TEST(AssetSourceDescription, clampsALongExtension) {
    EXPECT_EQ("host=host ext=aaaaaaaa", describeAssetSource(StringBox::fromCString("https://host/a.aaaaaaaaaaaaaaaa")));
}

TEST(AssetSourceDescription, reportsNoHostForASchemelessSource) {
    EXPECT_EQ("host=none", describeAssetSource(StringBox::fromCString("{\"v\":\"5.7.4\",\"layers\":[]}")));
}

TEST(AssetSourceDescription, describesAWrapperUrlByItsInnerTarget) {
    const auto described = describeAssetSource(StringBox::fromCString(
        "composer-encrypted-thumbnail://?url=https%3A%2F%2Fcf-st.sc-cdn.net%2Fthumb.jpg&key=THEKEY&iv=THEIV"));

    EXPECT_EQ("host=cf-st.sc-cdn.net ext=jpg wrapper=composer-encrypted-thumbnail", described);
    EXPECT_EQ(std::string::npos, described.find("THEKEY"));
    EXPECT_EQ(std::string::npos, described.find("THEIV"));
}

TEST(AssetSourceDescription, reportsTheWrapperWhenItCarriesNoInnerTarget) {
    EXPECT_EQ("host=none wrapper=composer-encrypted-image",
              describeAssetSource(StringBox::fromCString("composer-encrypted-image://?contentObject=AQID")));
    EXPECT_EQ("host=none wrapper=composer-encrypted-image",
              describeAssetSource(StringBox::fromCString("composer-encrypted-image://profile-icon")));
}

TEST(AssetSourceDescription, ignoresAnInnerTargetThatIsNotAWholeQueryParameter) {
    EXPECT_EQ("host=none wrapper=composer-encrypted-thumbnail",
              describeAssetSource(
                  StringBox::fromCString("composer-encrypted-thumbnail://?contenturl=https%3A%2F%2Fhost%2Fa.jpg")));
}

TEST(AssetSourceDescription, capsALongHost) {
    const std::string url = "https://" + std::string(2000, 'a') + "/sticker.jpg";

    EXPECT_EQ("host=" + std::string(64, 'a') + " ext=jpg", describeAssetSource(StringBox::fromString(url)));
}

TEST(AssetSourceDescription, capsALongWrappedUrlBeforeDecodingIt) {
    const std::string url =
        "composer-encrypted-thumbnail://?url=https%3A%2F%2F" + std::string(2000, 'b') + "%2Fthumb.jpg&key=THEKEY";

    const auto described = describeAssetSource(StringBox::fromString(url));

    EXPECT_EQ("host=" + std::string(64, 'b') + " ext=none wrapper=composer-encrypted-thumbnail", described);
    EXPECT_EQ(std::string::npos, described.find("THEKEY"));
}
