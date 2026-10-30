//
//  JavaScriptErrorStackTrace.cpp
//  valdi
//
//  Created by Simon Corsin on 6/11/21.
//

#include "valdi/runtime/JavaScript/JavaScriptErrorStackTrace.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"

namespace Valdi {

static ReferenceInfo makeReferenceInfo() {
    static auto kError = STRING_LITERAL("error");
    return ReferenceInfoBuilder().withObject(kError).build();
}

JavaScriptErrorStackTrace::JavaScriptErrorStackTrace(IJavaScriptContext& jsContext,
                                                     const JSValue& jsValue,
                                                     JSExceptionTracker& exceptionTracker)
    : _error(jsContext, jsValue, makeReferenceInfo(), exceptionTracker, false) {}

JavaScriptErrorStackTrace::~JavaScriptErrorStackTrace() = default;

StringBox JavaScriptErrorStackTrace::getStackTrace() {
    if (_stackTrace.isEmpty()) {
        auto taskScheduler = _error.getTaskScheduler();
        if (taskScheduler != nullptr) {
            constexpr auto reason = JsThreadDispatchReason::ErrorStackTrace;
            taskScheduler->dispatchOnJsThreadSync(reason, [&](const JavaScriptEntryParameters& jsEntry) {
                auto& exceptionTracker = jsEntry.exceptionTracker;
                auto jsError = _error.getJsValue(jsEntry.jsContext, exceptionTracker);

                if (exceptionTracker) {
                    auto retainedJsError = JSValueRef::makeRetained(jsEntry.jsContext, jsError);
                    if (jsEntry.jsContext.getListener() != nullptr) {
                        retainedJsError = jsEntry.jsContext.getListener()->symbolicateError(retainedJsError);
                    }

                    auto jsStack =
                        jsEntry.jsContext.getObjectProperty(retainedJsError.get(), "stack", exceptionTracker);
                    if (exceptionTracker) {
                        _stackTrace = jsEntry.jsContext.valueToString(jsStack.get(), exceptionTracker);
                    }
                    exceptionTracker.clearError();

                    auto jsBundleName = jsEntry.jsContext.getObjectProperty(retainedJsError.get(),
                                                                            "stackBundleName",
                                                                            exceptionTracker);
                    if (exceptionTracker && !jsEntry.jsContext.isValueUndefined(jsBundleName.get()) &&
                        !jsEntry.jsContext.isValueNull(jsBundleName.get())) {
                        _stackBundleName = jsEntry.jsContext.valueToString(jsBundleName.get(), exceptionTracker);
                    }
                    exceptionTracker.clearError();
                } else {
                    exceptionTracker.clearError();
                }
            });
        }
    }

    return _stackTrace;
}

StringBox JavaScriptErrorStackTrace::getStackBundleName() {
    getStackTrace();
    return _stackBundleName;
}

} // namespace Valdi
