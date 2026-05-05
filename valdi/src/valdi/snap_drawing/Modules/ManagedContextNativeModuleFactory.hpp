//
//  ManagedContextNativeModuleFactory.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 6/17/24.
//

#pragma once

#include "snap_drawing/cpp/Utils/Aliases.hpp"
#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"

namespace Valdi {
class ValueFunctionCallContext;
class ViewManagerContext;
} // namespace Valdi

namespace snap::drawing {

class Runtime;

class ManagedContextNativeModuleFactory final : public Valdi::SharedPtrRefCountable,
                                                public snap::valdi_core::ModuleFactory {
public:
    ManagedContextNativeModuleFactory(Valdi::Function<Ref<Runtime>()> _runtimeProvider,
                                      Valdi::Function<Ref<Valdi::ViewManagerContext>()> viewManagerContextProvider);
    ~ManagedContextNativeModuleFactory() final;

    Valdi::StringBox getModulePath() final;
    Valdi::Value loadModule() final;

private:
    Valdi::Function<Ref<Runtime>()> _runtimeProvider;
    Valdi::Function<Ref<Valdi::ViewManagerContext>()> _viewManagerContextProvider;

    Valdi::Value createValdiContextWithSnapDrawing(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value destroyValdiContextWithSnapDrawing(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value layoutAsync(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value measureAsync(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value drawFrame(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value drawFrameSync(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value disposeFrame(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value rasterFrame(const Valdi::ValueFunctionCallContext& callContext);
};

} // namespace snap::drawing
