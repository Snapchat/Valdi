//
//  UnicodeModuleFactory.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/9/24.
//

#pragma once

#include "valdi/runtime/JavaScript/Modules/JavaScriptModuleFactory.hpp"

namespace Valdi {

class IJavaScriptContext;

class UnicodeModuleFactory : public JavaScriptModuleFactory {
public:
    UnicodeModuleFactory();
    ~UnicodeModuleFactory() override;

    StringBox getModulePath() const final;
    JSValueRef loadModule(IJavaScriptContext& context,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          JSExceptionTracker& exceptionTracker) override;

private:
    static JSValueRef strToCodepoints(const Ref<StaticString>& str,
                                      bool normalize,
                                      bool disableCategorization,
                                      JSFunctionNativeCallContext& callContext);
    static JSValueRef codepointsToStr(JSValue codepoints,
                                      bool normalize,
                                      bool disableCategorization,
                                      JSFunctionNativeCallContext& callContext);
    static JSValueRef encodeString(const Ref<StaticString>& str,
                                   int32_t encoding,
                                   JSFunctionNativeCallContext& callContext);
    static JSValueRef decodeIntoString(const JSTypedArray& buffer,
                                       int32_t encoding,
                                       JSFunctionNativeCallContext& callContext);
};

} // namespace Valdi
