//
//  JSFunctionWithCallable.hpp
//  valdi
//
//  Created by Simon Corsin on 5/11/21.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"

namespace Valdi {

using JSFunctionCallable = Function<JSValueRef(JSFunctionNativeCallContext&)>;

/**
 a JSFunction backed by an anonymous callable, represented as a Valdi::Function.
 */
class JSFunctionWithCallable : public JSFunction {
public:
    JSFunctionWithCallable(const ReferenceInfoBuilder& referenceInfoBuilder,
                           JSFunctionCallable&& callable,
                           const StringBox& anrAttribution = StringBox());
    ~JSFunctionWithCallable() override;

    const ReferenceInfo& getReferenceInfo() const override;
    /** Stable label carried when this callback is dispatched back to its owning runtime. */
    const StringBox& getANRAttribution() const;
    JSValueRef operator()(JSFunctionNativeCallContext& callContext) noexcept override;

private:
    ReferenceInfo _referenceInfo;
    JSFunctionCallable _callable;
    StringBox _anrAttribution;
};

} // namespace Valdi
