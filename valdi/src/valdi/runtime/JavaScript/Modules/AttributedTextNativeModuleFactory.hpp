#pragma once

#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace Valdi {

class ColorPaletteManager;
class ILogger;
class ValueFunctionCallContext;

class AttributedTextNativeModuleFactory : public Valdi::SharedPtrRefCountable, public snap::valdi_core::ModuleFactory {
public:
    AttributedTextNativeModuleFactory(const Ref<ColorPaletteManager>& colorPaletteManager, ILogger& logger);
    ~AttributedTextNativeModuleFactory() override;

    StringBox getModulePath() override;
    Value loadModule() override;

private:
    Ref<ColorPaletteManager> _colorPaletteManager;
    ILogger& _logger;

    Value makeNativeAttributedText(const ValueFunctionCallContext& callContext) const;
};

} // namespace Valdi
