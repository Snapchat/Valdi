//
//  FontManagerNativeModuleFactory.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 5/16/24.
//

#include "valdi/snap_drawing/Modules/FontManagerNativeModuleFactory.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "valdi/snap_drawing/Runtime.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithMethod.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

namespace snap::drawing {

FontManagerNativeModuleFactory::FontManagerNativeModuleFactory(Valdi::Function<Ref<Runtime>()> runtimeProvider)
    : _runtimeProvider(std::move(runtimeProvider)) {}

FontManagerNativeModuleFactory::~FontManagerNativeModuleFactory() = default;

Valdi::StringBox FontManagerNativeModuleFactory::getModulePath() {
    return STRING_LITERAL("drawing/src/FontManagerNative");
}

Valdi::Value FontManagerNativeModuleFactory::loadModule() {
    Valdi::Value out;

    Valdi::ValueFunctionMethodBinder<FontManagerNativeModuleFactory> binder(this, out);
    binder.bind("getDefaultFontManager", &FontManagerNativeModuleFactory::getDefaultFontManager);
    binder.bind("makeScopedFontManager", &FontManagerNativeModuleFactory::makeScopedFontManager);
    binder.bind("registerFontFromData", &FontManagerNativeModuleFactory::registerFontFromData);
    binder.bind("registerFontFromFilePath", &FontManagerNativeModuleFactory::registerFontFromFilePath);

    return out;
}

Valdi::Value FontManagerNativeModuleFactory::getDefaultFontManager(const Valdi::ValueFunctionCallContext& callContext) {
    return Valdi::Value(_runtimeProvider()->getResources()->getFontManager());
}

static Valdi::Ref<IFontManager> getFontManagerFromCallContext(const Valdi::ValueFunctionCallContext& callContext) {
    return callContext.getParameter(0).checkedToValdiObject<IFontManager>(callContext.getExceptionTracker());
}

Valdi::Value FontManagerNativeModuleFactory::makeScopedFontManager(const Valdi::ValueFunctionCallContext& callContext) {
    auto fontManager = getFontManagerFromCallContext(callContext);
    if (fontManager == nullptr) {
        return Valdi::Value::undefined();
    }

    return Valdi::Value(fontManager->makeScoped());
}

static Valdi::Value doRegisterFont(const Valdi::ValueFunctionCallContext& callContext,
                                   const Valdi::StringBox& fontName,
                                   const Ref<snap::drawing::LoadableTypeface>& loadableTypeface,
                                   bool canUseAsFallback) {
    auto fontManager = getFontManagerFromCallContext(callContext);
    if (fontManager != nullptr) {
        auto fontWeightStr = callContext.getParameter(2).checkedTo<Valdi::StringBox>(callContext.getExceptionTracker());
        if (!callContext.getExceptionTracker()) {
            return Valdi::Value();
        }
        auto fontStyleStr = callContext.getParameter(3).checkedTo<Valdi::StringBox>(callContext.getExceptionTracker());
        if (!callContext.getExceptionTracker()) {
            return Valdi::Value();
        }

        auto fontWeight = FontStyle::parseWeight(fontWeightStr.toStringView());
        if (!fontWeight) {
            callContext.getExceptionTracker().onError(fontWeight.moveError());
            return Valdi::Value();
        }
        auto fontSlant = FontStyle::parseSlant(fontStyleStr.toStringView());
        if (!fontSlant) {
            callContext.getExceptionTracker().onError(fontWeight.moveError());
            return Valdi::Value();
        }

        fontManager->registerTypeface(fontName,
                                      FontStyle(FontWidthNormal, fontWeight.value(), fontSlant.value()),
                                      canUseAsFallback,
                                      loadableTypeface);
    }

    return Valdi::Value::undefined();
}

// Optional trailing parameter, so callers that pass 5 arguments keep the previous behavior of
// registering a non-fallback family. Guard on arity rather than converting the missing parameter:
// getParameterAsBool runs a checked conversion, which would record a type error on `undefined`.
static bool getCanUseAsFallback(const Valdi::ValueFunctionCallContext& callContext) {
    constexpr size_t kCanUseAsFallbackIndex = 5;

    if (callContext.getParametersSize() <= kCanUseAsFallbackIndex) {
        return false;
    }

    return callContext.getParameterAsBool(kCanUseAsFallbackIndex);
}

Valdi::Value FontManagerNativeModuleFactory::registerFontFromData(const Valdi::ValueFunctionCallContext& callContext) {
    auto fontData =
        callContext.getParameter(4).checkedTo<Ref<Valdi::ValueTypedArray>>(callContext.getExceptionTracker());
    if (fontData == nullptr) {
        return Valdi::Value();
    }

    auto fontName = callContext.getParameter(1).checkedTo<Valdi::StringBox>(callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return Valdi::Value();
    }

    return doRegisterFont(callContext,
                          fontName,
                          LoadableTypeface::fromBytes(fontName, fontData->getBuffer()),
                          getCanUseAsFallback(callContext));
}

Valdi::Value FontManagerNativeModuleFactory::registerFontFromFilePath(
    const Valdi::ValueFunctionCallContext& callContext) {
    // StringBox is not pointer-like: comparing it to nullptr picks operator==(std::string_view),
    // which builds a string_view from a null char* and crashes in strlen. Check the exception
    // tracker instead, as the fontName conversion below does.
    auto fontPath = callContext.getParameter(4).checkedTo<Valdi::StringBox>(callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return Valdi::Value();
    }

    auto fontName = callContext.getParameter(1).checkedTo<Valdi::StringBox>(callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return Valdi::Value();
    }

    // fromFile takes (fontName, filePath). This passed fontName for both, so the typeface was
    // loaded from a path named after the font family and registration silently produced nothing
    // usable.
    return doRegisterFont(
        callContext, fontName, LoadableTypeface::fromFile(fontName, fontPath), getCanUseAsFallback(callContext));
}

} // namespace snap::drawing
