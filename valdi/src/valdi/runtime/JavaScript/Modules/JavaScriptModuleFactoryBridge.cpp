//
//  JavaScriptModuleFactory.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 12/13/22.
//

#include "valdi/runtime/JavaScript/Modules/JavaScriptModuleFactoryBridge.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace Valdi {

JavaScriptModuleFactoryBridge::JavaScriptModuleFactoryBridge(
    const Shared<snap::valdi_core::ModuleFactory>& moduleFactory)
    : _moduleFactory(moduleFactory) {}
JavaScriptModuleFactoryBridge::~JavaScriptModuleFactoryBridge() = default;

StringBox JavaScriptModuleFactoryBridge::getModulePath() const {
    return _moduleFactory->getModulePath();
}

JSValueRef JavaScriptModuleFactoryBridge::loadModule(IJavaScriptContext& jsContext,
                                                     const ReferenceInfoBuilder& referenceInfoBuilder,
                                                     JSExceptionTracker& exceptionTracker) {
    // Native factories can block the JS thread here (e.g. the iOS device module waits for the main thread to read
    // screen settings), and that wait is otherwise only visible as unexplained time in the importing loadJsModule.
    VALDI_TRACE_META("Valdi.loadNativeModule", _moduleFactory->getModulePath());
    auto value = _moduleFactory->loadModule();
    return valueToJSValue(jsContext, value, referenceInfoBuilder.withReturnValue(), exceptionTracker);
}

} // namespace Valdi
