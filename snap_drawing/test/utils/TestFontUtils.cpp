#include "TestFontUtils.hpp"

#include "TestDataUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace snap::drawing {

Ref<Font> loadTestFont(const Ref<FontManager>& fontManager,
                       std::string_view fontFamilyName,
                       FontStyle fontStyle,
                       const std::string& filename,
                       bool canUseAsFallback,
                       Scalar fontSize,
                       double scale) {
    auto testData = getTestData(filename);
    SC_ASSERT(testData.success(), testData.description());

    auto familyName = Valdi::StringCache::getGlobal().makeString(fontFamilyName);
    fontManager->registerTypeface(familyName, fontStyle, canUseAsFallback, testData.value());

    auto font = fontManager->getFontWithNameAndSize(familyName, fontSize, scale, true);
    SC_ASSERT(font.success(), font.description());
    return font.moveValue();
}

} // namespace snap::drawing
