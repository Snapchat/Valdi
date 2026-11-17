//
//  FontManager.hpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/29/20.
//

#pragma once

#include "snap_drawing/cpp/Text/Character.hpp"
#include "snap_drawing/cpp/Text/Font.hpp"
#include "snap_drawing/cpp/Text/IFontManager.hpp"
#include "snap_drawing/cpp/Text/LoadableTypeface.hpp"
#include "snap_drawing/cpp/Text/TypefaceRegistry.hpp"
#include "snap_drawing/cpp/Utils/Aliases.hpp"

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include "include/core/SkFontMgr.h"

#include <optional>
#include <vector>

namespace Valdi {
class ILogger;
}

namespace snap::drawing {

class TextShaper;
class FontFamily;
class FontFamilyWithLoadableTypefaces;

struct LoadResult {
    std::vector<String> availableFonts;
    bool activeInitialization;
};

class IFontManagerListener : public Valdi::SimpleRefCountable {
public:
    virtual Ref<LoadableTypeface> resolveTypefaceWithName(const String& name) = 0;
};

class FontManager : public IFontManager, protected ITypefaceRegistryListener {
public:
    FontManager(Valdi::ILogger& logger, bool enableTextShaperCache = true);
    ~FontManager() override;

    /**
     * Calling load() can result with either active or passive initialization of Skia's font manager.
     * An active initialization is the first initialization which may be computation/io intensive depending on the
     * underlying platform. A passive initialization is a consecutive initialization that relies on data collected
     * in the active initialization and is *not* computation intensive.
     *
     * @return pair of available font families and whether Skia's initialization was active or not.
     * The active/passive initialization is used for metrics reporting purposes only.
     */
    LoadResult load();

    std::vector<String> getAvailableFonts();

    Valdi::Result<Valdi::Ref<Font>> getFontForName(const String& fontName, double scale);
    Valdi::Result<Valdi::Ref<Font>> getFontWithNameAndSize(const String& fontName,
                                                           Scalar fontSize,
                                                           double scale,
                                                           bool respectDynamicType);
    Valdi::Result<Valdi::Ref<Typeface>> getTypefaceWithName(const String& fontName) override;
    Valdi::Result<Valdi::Ref<Font>> getFontWithFamilyNameAndStyle(
        const String& familyName, FontStyle fontStyle, Scalar fontSize, double scale, bool respectDynamicType);

    Valdi::Result<Valdi::Ref<Typeface>> getTypefaceWithFamilyNameAndStyle(const String& familyName,
                                                                          FontStyle fontStyle) override;

    Valdi::Result<Valdi::Ref<Font>> getDefaultFont();
    Valdi::Result<Valdi::Ref<Font>> getDefaultFont(Scalar fontSize, double scale);

    Valdi::Result<Ref<Font>> getFont(const Ref<Typeface>& typeface,
                                     FontStyle fontStyle,
                                     Scalar fontSize,
                                     double scale,
                                     bool respectDynamicType) override;
    Valdi::Result<Ref<Font>> getCompatibleFont(const Ref<Font>& originalFont,
                                               const char** bcp47,
                                               size_t bcp47Count,
                                               Character character);
    Valdi::Result<Ref<Font>> getCompatibleFont(const String& familyName,
                                               FontStyle fontStyle,
                                               Scalar fontSize,
                                               double scale,
                                               const char** bcp47,
                                               size_t bcp47Count,
                                               Character character);

    Valdi::Result<Ref<Font>> getEmojiFont(Scalar fontSize, double scale);

    Ref<IFontManager> makeScoped() override;

    void registerTypeface(const String& fontFamilyName,
                          FontStyle fontStyle,
                          bool canUseAsFallback,
                          const Valdi::BytesView& fontData);

    void registerTypeface(const String& fontFamilyName,
                          FontStyle fontStyle,
                          bool canUseAsFallback,
                          const Ref<LoadableTypeface>& loadableTypeface) override;

    const Ref<TextShaper>& getTextShaper() const;

    const sk_sp<SkFontMgr>& getSkValue();

    /** Sets hinting for subsequently resolved fonts. Configure before creating views. Defaults to Normal. */
    void setFontHinting(SkFontHinting hinting);

    void setListener(const Ref<IFontManagerListener>& listener);

protected:
    Ref<FontFamily> onFontFamilyMissing(const String& familyName) override;

private:
    mutable Valdi::Mutex _mutex;
    [[maybe_unused]] Valdi::ILogger& _logger;
    sk_sp<SkFontMgr> _fontManager;

    TypefaceRegistry _typefaceRegistry;
    Valdi::FlatMap<Character, Ref<FontFamily>> _fallbackFontFamilies;
    FontStyle _defaultFontStyle;
    Ref<TextShaper> _textShaper;
    Valdi::StringBox _defaultFontFamilyName;
    Ref<IFontManagerListener> _listener;
    SkFontHinting _fontHinting = SkFontHinting::kNormal;

    std::unique_lock<Valdi::Mutex> lock();
    std::unique_lock<Valdi::Mutex> lock(bool shouldInitIfNeeded, bool* didInit);

    Valdi::Result<Valdi::Ref<Font>> getDefaultFontWithStyle(std::unique_lock<Valdi::Mutex>& lock,
                                                            FontStyle fontStyle,
                                                            Scalar fontSize,
                                                            double scale);

    Ref<FontFamily> resolveFallbackFontFamilyForCharacter(std::unique_lock<Valdi::Mutex>& lock, Character character);
    Valdi::Result<Valdi::Ref<Font>> getFontFromFamily(std::unique_lock<Valdi::Mutex>& lock,
                                                      const Ref<FontFamily>& fontFamily,
                                                      FontStyle fontStyle,
                                                      Scalar fontSize,
                                                      double scale,
                                                      bool respectDynamicType);
    Valdi::Result<Valdi::Ref<Font>> getFontWithFamilyNameAndStyle(std::unique_lock<Valdi::Mutex>& lock,
                                                                  const String& familyName,
                                                                  FontStyle fontStyle,
                                                                  Scalar fontSize,
                                                                  double scale,
                                                                  bool respectDynamicType);

    void onFontResolveFailed(const String& fontName, const Valdi::Error& error);
    std::vector<String> doGetAvailableFonts(std::unique_lock<Valdi::Mutex>& lock) const;

    bool doLoad(std::unique_lock<Valdi::Mutex>& lock);

    Valdi::Ref<Font> getFontWithTypefaceAndSize(std::unique_lock<Valdi::Mutex>& /*lock*/,
                                                const Ref<Typeface>& typeface,
                                                Scalar fontSize,
                                                double scale,
                                                bool respectDynamicType);
};

} // namespace snap::drawing
