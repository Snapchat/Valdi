//
//  JSFunctionWithCallable.cpp
//  valdi
//
//  Created by Simon Corsin on 5/11/21.
//

#include "valdi/runtime/JavaScript/JSFunctionWithCallable.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"

namespace Valdi {

JSFunctionWithCallable::JSFunctionWithCallable(const ReferenceInfoBuilder& referenceInfoBuilder,
                                               JSFunctionCallable&& callable,
                                               const StringBox& anrAttribution)
    : _referenceInfo(referenceInfoBuilder.asFunction().build()),
      _callable(std::move(callable)),
      _anrAttribution(JavaScriptANRAttribution::boundedLabel(anrAttribution)) {}

JSFunctionWithCallable::~JSFunctionWithCallable() = default;

const ReferenceInfo& JSFunctionWithCallable::getReferenceInfo() const {
    return _referenceInfo;
}

const StringBox& JSFunctionWithCallable::getANRAttribution() const {
    return _anrAttribution;
}

JSValueRef JSFunctionWithCallable::operator()(JSFunctionNativeCallContext& callContext) noexcept {
    auto* runtime = _anrAttribution.isEmpty() ?
                        nullptr :
                        dynamic_cast<JavaScriptRuntime*>(callContext.getContext().getTaskScheduler());
    ScopedNativeCallActivity activity(runtime, _anrAttribution);
    return _callable(callContext);
}

} // namespace Valdi
