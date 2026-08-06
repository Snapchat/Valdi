//
//  FontManager.cpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/29/20.
//

#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Text/FontFamily.hpp"
#include "snap_drawing/cpp/Text/FontFamilyWithLoadableTypefaces.hpp"
#include "snap_drawing/cpp/Text/FontFamilyWithStyleSet.hpp"
#include "snap_drawing/cpp/Text/SkFontMgrSingleton.hpp"
#include "snap_drawing/cpp/Text/TextShaper.hpp"

#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace snap::drawing {

class ScopedFontManager : public IFontManager {
public:
    ScopedFontManager(SkFontMgr& fontMgr, Valdi::ILogger& logger, const Ref<IFontManager>& parent)
        : _typefaceRegistry(logger, nullptr), _fontMgr(fontMgr), _parent(parent) {}

    ~ScopedFontManager() override = default;

    void registerTypeface(const String& fontFamilyName,
                          FontStyle fontStyle,
                          bool canUseAsFallback,
                          const Ref<LoadableTypeface>& loadableTypeface) override {
        std::lock_guard<Valdi::Mutex> guard(_mutex);
        _typefaceRegistry.registerTypeface(fontFamilyName, fontStyle, canUseAsFallback, loadableTypeface);
    }

    Ref<IFontManager> makeScoped() override {
        return Valdi::makeShared<ScopedFontManager>(
            _fontMgr, _typefaceRegistry.getLogger(), Valdi::strongSmallRef(this));
    }

    Valdi::Result<Ref<Font>> getFont(const Ref<Typeface>& typeface,
                                     FontStyle fontStyle,
                                     Scalar fontSize,
                                     double scale,
                                     bool respectDynamicType) override {
        // Not implemented locally, we don't yet need this functionality
        return _parent->getFont(typeface, fontStyle, fontSize, scale, respectDynamicType);
    }

    Valdi::Result<Valdi::Ref<Typeface>> getTypefaceWithName(const String& name) override {
        {
            std::lock_guard<Valdi::Mutex> guard(_mutex);
            auto typeface = _typefaceRegistry.getTypefaceWithName(_fontMgr, name, nullptr);
            if (typeface) {
                return typeface.moveValue();
            }
        }

        return _parent->getTypefaceWithName(name);
    }

    Valdi::Result<Valdi::Ref<Typeface>> getTypefaceWithFamilyNameAndStyle(const String& familyName,
                                                                          FontStyle fontStyle) override {
        {
            std::lock_guard<Valdi::Mutex> guard(_mutex);
            auto typeface = _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(_fontMgr, familyName, fontStyle);
            if (typeface) {
                return typeface.moveValue();
            }
        }

        return _parent->getTypefaceWithFamilyNameAndStyle(familyName, fontStyle);
    }

private:
    TypefaceRegistry _typefaceRegistry;
    SkFontMgr& _fontMgr;
    Ref<IFontManager> _parent;
    Valdi::Mutex _mutex;
};

#if __ANDROID__
const char* kFallbackDefaultFontFamilyName = "helvetica";
#else
const char* kFallbackDefaultFontFamilyName = "Helvetica";
#endif

static Valdi::StringBox getFamilyNameAtIndex(SkFontMgr* mgr, size_t index) {
    SkString familyName;
    mgr->getFamilyName(static_cast<int>(index), &familyName);

    return Valdi::StringCache::makeStringFromCLiteral(familyName.c_str());
}

FontManager::FontManager(Valdi::ILogger& logger, bool enableTextShaperCache)
    : _logger(logger),
      _typefaceRegistry(logger, this),
      _defaultFontStyle(FontWidthNormal, FontWeightNormal, FontSlantUpright),
      _textShaper(TextShaper::make(enableTextShaperCache)) {}

FontManager::~FontManager() = default;

LoadResult FontManager::load() {
    // Acquiring the lock will trigger the load if it weren't loaded yet
    bool activeInit;
    auto guard = lock(true, &activeInit);
    return {doGetAvailableFonts(guard), activeInit};
}

bool FontManager::doLoad(std::unique_lock<Valdi::Mutex>& /*lock*/) {
    VALDI_TRACE("SnapDrawing.loadFontManager");

    bool isFirst = false;
    static std::once_flag sOnceFlag;
    std::call_once(sOnceFlag, [&isFirst] { isFirst = true; });
    _fontManager = snap_drawing::getSkFontMgrSingleton();

    // Query the system for the default font
    auto skTypeface = sk_sp<SkTypeface>(_fontManager->matchFamilyStyle(nullptr, _defaultFontStyle.getSkValue()));
    if (skTypeface == nullptr) {
        // On Android (at least on some devices) the flow lands here, so use the fallback default
        _defaultFontFamilyName = Valdi::StringCache::makeStringFromCLiteral(kFallbackDefaultFontFamilyName);
    } else {
        SkString familyName;
        skTypeface->getFamilyName(&familyName);
        _defaultFontFamilyName = Valdi::StringCache::makeStringFromCLiteral(familyName.c_str());
    }
    // This will create a FontFamily object for the default font and cache it
    _typefaceRegistry.getFontFamilyByFamilyName(_defaultFontFamilyName);

    auto families = static_cast<size_t>(_fontManager->countFamilies());
    for (size_t i = 0; i < families; i++) {
        auto familyName = getFamilyNameAtIndex(_fontManager.get(), i);
        _typefaceRegistry.registerFamilyName(familyName);
    }
    return isFirst;
}

std::vector<String> FontManager::getAvailableFonts() {
    auto guard = lock();
    return doGetAvailableFonts(guard);
}

std::vector<String> FontManager::doGetAvailableFonts(std::unique_lock<Valdi::Mutex>& /*lock*/) const {
    return _typefaceRegistry.getAllAvailableTypefaceNames();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Valdi::Ref<Font> FontManager::getFontWithTypefaceAndSize(std::unique_lock<Valdi::Mutex>& /*lock*/,
                                                         const Ref<Typeface>& typeface,
                                                         Scalar fontSize,
                                                         double scale,
                                                         bool respectDynamicType) {
    // TODO(simon): Caching of Font objects
    auto font = Valdi::makeShared<Font>(
        Valdi::strongSmallRef(this), Ref<Typeface>(typeface), fontSize, scale, respectDynamicType);
    return Valdi::Ref<Font>(std::move(font));
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getDefaultFontWithStyle(std::unique_lock<Valdi::Mutex>& lock,
                                                                     FontStyle fontStyle,
                                                                     Scalar fontSize,
                                                                     double scale) {
    return getFontWithFamilyNameAndStyle(lock, _defaultFontFamilyName, fontStyle, fontSize, scale, true);
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getFont(
    const Valdi::Ref<Typeface>& typeface, FontStyle fontStyle, Scalar fontSize, double scale, bool respectDynamicType) {
    auto guard = lock();
    if (typeface->isCustom()) {
        // TODO(simon): Is this still necessary now that we have the font family system?
        return getFontWithTypefaceAndSize(guard, typeface, fontSize, scale, respectDynamicType);
    } else {
        return getFontWithFamilyNameAndStyle(
            guard, typeface->familyName(), fontStyle, fontSize, scale, respectDynamicType);
    }
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getFontWithFamilyNameAndStyle(std::unique_lock<Valdi::Mutex>& lock,
                                                                           const String& familyName,
                                                                           FontStyle fontStyle,
                                                                           Scalar fontSize,
                                                                           double scale,
                                                                           bool respectDynamicType) {
    auto typeface = _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(*_fontManager, familyName, fontStyle);
    if (!typeface) {
        return typeface.moveError();
    }

    return getFontWithTypefaceAndSize(lock, typeface.value(), fontSize, scale, respectDynamicType);
}

Valdi::Result<Valdi::Ref<Typeface>> FontManager::getTypefaceWithName(const String& fontName) {
    static auto kSystemFontKey = STRING_LITERAL("system");
    static auto kSystemFontMediumKey = STRING_LITERAL("system-medium");
    static auto kSystemFontSemiBoldKey = STRING_LITERAL("system-semibold");
    static auto kSystemFontDemiBoldKey = STRING_LITERAL("system-demi-bold");
    static auto kSystemFontBoldKey = STRING_LITERAL("system-bold");
    static auto kSystemFontItalicKey = STRING_LITERAL("system-italic");
    static auto kSystemFontMediumItalicKey = STRING_LITERAL("system-medium-italic");
    static auto kSystemFontSemiBoldItalicKey = STRING_LITERAL("system-semibold-italic");
    static auto kSystemFontDemiBoldItalicKey = STRING_LITERAL("system-demi-bold-italic");
    static auto kSystemFontBoldItalicKey = STRING_LITERAL("system-bold-italic");

    auto guard = lock();
    if (fontName == kSystemFontKey) {
        return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(
            *_fontManager, _defaultFontFamilyName, _defaultFontStyle);
    } else if (fontName == kSystemFontMediumKey) {
        return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(
            *_fontManager, _defaultFontFamilyName, FontStyle(FontWidthNormal, FontWeightMedium, FontSlantUpright));
    } else if (fontName == kSystemFontSemiBoldKey || fontName == kSystemFontDemiBoldKey) {
        return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(
            *_fontManager, _defaultFontFamilyName, FontStyle(FontWidthNormal, FontWeightSemiBold, FontSlantUpright));
    } else if (fontName == kSystemFontBoldKey) {
        return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(
            *_fontManager, _defaultFontFamilyName, FontStyle(FontWidthNormal, FontWeightBold, FontSlantUpright));
    } else if (fontName == kSystemFontItalicKey) {
        return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(
            *_fontManager, _defaultFontFamilyName, FontStyle(FontWidthNormal, FontWeightNormal, FontSlantItalic));
    } else if (fontName == kSystemFontMediumItalicKey) {
        return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(
            *_fontManager, _defaultFontFamilyName, FontStyle(FontWidthNormal, FontWeightMedium, FontSlantItalic));
    } else if (fontName == kSystemFontSemiBoldItalicKey || fontName == kSystemFontDemiBoldItalicKey) {
        return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(
            *_fontManager, _defaultFontFamilyName, FontStyle(FontWidthNormal, FontWeightSemiBold, FontSlantItalic));
    } else if (fontName == kSystemFontBoldItalicKey) {
        return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(
            *_fontManager, _defaultFontFamilyName, FontStyle(FontWidthNormal, FontWeightBold, FontSlantItalic));
    }
    FontStyle fontStyle = _defaultFontStyle;

    auto result = _typefaceRegistry.getTypefaceWithName(*_fontManager, fontName, &fontStyle);

    if (result) {
        return result;
    }

    onFontResolveFailed(fontName, Valdi::Error("Font is not available"));
    return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(*_fontManager, _defaultFontFamilyName, fontStyle);
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getFontWithNameAndSize(const String& fontName,
                                                                    Scalar fontSize,
                                                                    double scale,
                                                                    bool respectDynamicType) {
    auto typefaceResult = getTypefaceWithName(fontName);
    if (!typefaceResult) {
        return typefaceResult.moveError();
    }

    auto guard = lock();

    return getFontWithTypefaceAndSize(guard, typefaceResult.value(), fontSize, scale, respectDynamicType);
}

Ref<FontFamily> FontManager::onFontFamilyMissing(const String& familyName) {
    if (_listener != nullptr) {
        auto loadableTypeface = _listener->resolveTypefaceWithName(familyName);
        if (loadableTypeface != nullptr) {
            auto family = makeShared<FontFamilyWithLoadableTypefaces>(_logger, familyName);
            // TODO(simon): This is hardcoded to have a single font style per family name, which
            // is not super flexible. We might need to improve this later.
            family->registerLoadableTypeface(FontStyle(FontWidthNormal, FontWeightNormal, FontSlantUpright),
                                             loadableTypeface);
            return family;
        }
    }

    const sk_sp<SkFontStyleSet> styleSet(_fontManager->matchFamily(familyName.getCStr()));
    if (styleSet == nullptr || styleSet->count() == 0) {
        return nullptr;
    }

    return Valdi::makeShared<FontFamilyWithStyleSet>(styleSet, familyName);
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getFontWithFamilyNameAndStyle(
    const String& familyName, FontStyle fontStyle, Scalar fontSize, double scale, bool respectDynamicType) {
    auto guard = lock();
    return getFontWithFamilyNameAndStyle(guard, familyName, fontStyle, fontSize, scale, respectDynamicType);
}

Valdi::Result<Valdi::Ref<Typeface>> FontManager::getTypefaceWithFamilyNameAndStyle(const String& familyName,
                                                                                   FontStyle fontStyle) {
    auto guard = lock();
    return _typefaceRegistry.getTypefaceWithFamilyNameAndStyle(*_fontManager, familyName, fontStyle);
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getCompatibleFont(const Ref<Font>& originalFont,
                                                               const char** bcp47,
                                                               size_t bcp47Count,
                                                               Character character) {
    return getCompatibleFont(originalFont->typeface()->familyName(),
                             originalFont->style(),
                             originalFont->size(),
                             originalFont->scale(),
                             bcp47,
                             bcp47Count,
                             character);
}

Ref<FontFamily> FontManager::resolveFallbackFontFamilyForCharacter(std::unique_lock<Valdi::Mutex>& /*lock*/,
                                                                   Character character) {
    auto fontFamily = _typefaceRegistry.getFontFamilyForCharacterAndStyle(*_fontManager, character, _defaultFontStyle);
    if (fontFamily != nullptr) {
        return fontFamily;
    }

    VALDI_TRACE("SnapDrawing.matchFamilyStyleCharacter");
    auto skTypeface =
        sk_sp<SkTypeface>(_fontManager->matchFamilyStyleCharacter(nullptr, SkFontStyle(), nullptr, 0, character));
    if (skTypeface == nullptr) {
        return nullptr;
    }

    SkString skFamilyName;
    skTypeface->getFamilyName(&skFamilyName);
    auto resolvedFamilyname = Valdi::StringCache::getGlobal().makeStringFromLiteral(skFamilyName.c_str());

    return _typefaceRegistry.getFontFamilyByFamilyName(resolvedFamilyname);
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getCompatibleFont(const String& /*familyName*/,
                                                               FontStyle fontStyle,
                                                               Scalar fontSize,
                                                               double scale,
                                                               const char** /*bcp47*/,
                                                               size_t /*bcp47Count*/,
                                                               Character character) {
    auto guard = lock();
    const auto& it = _fallbackFontFamilies.find(character);
    if (it != _fallbackFontFamilies.end()) {
        return getFontFromFamily(guard, it->second, fontStyle, fontSize, scale, true);
    }

    auto fontFamily = resolveFallbackFontFamilyForCharacter(guard, character);
    _fallbackFontFamilies[character] = fontFamily;

    return getFontFromFamily(guard, fontFamily, fontStyle, fontSize, scale, true);
}

Valdi::Result<Ref<Font>> FontManager::getEmojiFont(Scalar fontSize, double scale) {
    Character emoji = 0x270C;
    return getCompatibleFont(String(), _defaultFontStyle, fontSize, scale, nullptr, 0, emoji);
}

Ref<IFontManager> FontManager::makeScoped() {
    return Valdi::makeShared<ScopedFontManager>(*getSkValue(), _logger, Valdi::strongSmallRef(this));
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getFontFromFamily(std::unique_lock<Valdi::Mutex>& lock,
                                                               const Ref<FontFamily>& fontFamily,
                                                               FontStyle fontStyle,
                                                               Scalar fontSize,
                                                               double scale,
                                                               bool respectDynamicType) {
    auto typeface = _typefaceRegistry.getTypefaceFromFamily(*_fontManager, fontFamily, fontStyle);
    if (!typeface) {
        return typeface.moveError();
    }

    return getFontWithTypefaceAndSize(lock, typeface.value(), fontSize, scale, respectDynamicType);
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getFontForName(const String& fontName, double scale) {
    auto components = fontName.split(' ', true);

    if (components.size() == 1) {
        auto guard = lock();
        if (components[0] == "title1") {
            return getDefaultFontWithStyle(guard, _defaultFontStyle, 25, scale);
        } else if (components[0] == "title2") {
            return getDefaultFontWithStyle(guard, _defaultFontStyle, 19, scale);
        } else if (components[0] == "title3") {
            return getDefaultFontWithStyle(guard, _defaultFontStyle, 17, scale);
        } else if (components[0] == "body") {
            return getDefaultFontWithStyle(guard, _defaultFontStyle, 14, scale);
        }
    }

    double fontSize = 12;
    bool respectDynamicType = true;
    auto fontNamePart = components[0];

    if (components.size() >= 2) {
        fontSize = Valdi::Value(components[1]).toDouble();
        if (components.size() >= 3 && components[2].lowercased() == "unscaled") {
            respectDynamicType = false;
        }
    }

    return getFontWithNameAndSize(fontNamePart, static_cast<Scalar>(fontSize), scale, respectDynamicType);
}

Valdi::Result<Ref<Font>> FontManager::getDefaultFont() {
    auto guard = lock();
    return getDefaultFontWithStyle(guard, _defaultFontStyle, 12, 1.0);
}

Valdi::Result<Valdi::Ref<Font>> FontManager::getDefaultFont(Scalar fontSize, double scale) {
    auto guard = lock();
    return getDefaultFontWithStyle(guard, _defaultFontStyle, fontSize, scale);
}

void FontManager::registerTypeface(const String& fontFamilyName,
                                   FontStyle fontStyle,
                                   bool canUseAsFallback,
                                   const Valdi::BytesView& fontData) {
    registerTypeface(
        fontFamilyName, fontStyle, canUseAsFallback, LoadableTypeface::fromBytes(fontFamilyName, fontData));
}

void FontManager::registerTypeface(const String& fontFamilyName,
                                   FontStyle fontStyle,
                                   bool canUseAsFallback,
                                   const Ref<LoadableTypeface>& loadableTypeface) {
    auto guard = lock(/* shouldInitIfNeeded */ false, nullptr);
    _typefaceRegistry.registerTypeface(fontFamilyName, fontStyle, canUseAsFallback, loadableTypeface);
    // Clear fallback font family cache, so that we can pickup our new typeface
    _fallbackFontFamilies.clear();
}

void FontManager::onFontResolveFailed(const String& fontName, const Valdi::Error& error) {
    VALDI_ERROR(_logger, "Failed to resolve font '{}': {}", fontName, error);
}

const Ref<TextShaper>& FontManager::getTextShaper() const {
    return _textShaper;
}

const sk_sp<SkFontMgr>& FontManager::getSkValue() {
    auto guard = lock();
    return _fontManager;
}

void FontManager::setListener(const Ref<IFontManagerListener>& listener) {
    auto guard = lock(/* shouldInitIfNeeded */ false, nullptr);
    _listener = listener;
}

std::unique_lock<Valdi::Mutex> FontManager::lock() {
    return lock(true, nullptr);
}

std::unique_lock<Valdi::Mutex> FontManager::lock(bool shouldInitIfNeeded, bool* didInit) {
    auto lock = std::unique_lock<Valdi::Mutex>(_mutex);
    auto alreadyInitialized = _fontManager != nullptr;
    if (!alreadyInitialized && shouldInitIfNeeded) {
        alreadyInitialized = !doLoad(lock);
    }

    if (didInit != nullptr) {
        *didInit = !alreadyInitialized;
    }
    return lock;
}

} // namespace snap::drawing
