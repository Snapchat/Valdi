#include "valdi/runtime/JavaScript/Modules/AttributedTextNativeModuleFactory.hpp"
#include "valdi/runtime/Attributes/TextAttributeValueParser.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

namespace Valdi {

AttributedTextNativeModuleFactory::AttributedTextNativeModuleFactory(const Ref<ColorPalette>& colorPalette,
                                                                     ILogger& logger)
    : _colorPalette(colorPalette), _logger(logger) {}

AttributedTextNativeModuleFactory::~AttributedTextNativeModuleFactory() = default;

StringBox AttributedTextNativeModuleFactory::getModulePath() {
    return STRING_LITERAL("drawing/src/AttributedTextNative");
}

Value AttributedTextNativeModuleFactory::loadModule() {
    return Value().setMapValue("makeNativeAttributedText",
                               Value(makeShared<ValueFunctionWithCallable>(
                                   [self = strongSmallRef(this)](const ValueFunctionCallContext& callContext) -> Value {
                                       return self->makeNativeAttributedText(callContext);
                                   })));
}

Value AttributedTextNativeModuleFactory::makeNativeAttributedText(const ValueFunctionCallContext& callContext) const {
    auto value = callContext.getParameter(0);
    auto result = TextAttributeValueParser::parse(*_colorPalette, value, _logger, true);
    if (!result) {
        callContext.getExceptionTracker().onError(result.moveError());
        return Value::undefined();
    }

    return result.value();
}

} // namespace Valdi