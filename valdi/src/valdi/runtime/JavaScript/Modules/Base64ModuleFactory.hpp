//
//  Base64ModuleFactory.hpp
//  valdi-ios
//

#pragma once

#include "valdi/runtime/JavaScript/Modules/JavaScriptModuleFactory.hpp"

namespace Valdi {

class Base64ModuleFactory : public JavaScriptModuleFactory {
public:
    Base64ModuleFactory();
    ~Base64ModuleFactory() override;

    StringBox getModulePath() const final;
    JSValueRef loadModule(IJavaScriptContext& context,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          JSExceptionTracker& exceptionTracker) override;

private:
    static JSValueRef encodeToBase64(const JSTypedArray& bytes, bool urlSafe, JSFunctionNativeCallContext& callContext);
    static JSValueRef decodeFromBase64(const Ref<StaticString>& base64, JSFunctionNativeCallContext& callContext);
};

} // namespace Valdi
