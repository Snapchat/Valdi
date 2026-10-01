//
//  JSValueRefHolder.hpp
//  valdi
//
//  Created by Simon Corsin on 5/11/21.
//

#pragma once

#include "utils/base/NonCopyable.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/Utils/Disposable.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <mutex>

namespace Valdi {

class JavaScriptTaskScheduler;
class Context;

class JSValueRefHolder : public snap::NonCopyable, public Disposable {
public:
    JSValueRefHolder(IJavaScriptContext& jsContext,
                     const JSValue& jsValue,
                     const ReferenceInfo& referenceInfo,
                     JSExceptionTracker& exceptionTracker,
                     bool captureStackTrace);
    ~JSValueRefHolder() override;

    Shared<JavaScriptTaskScheduler> getTaskScheduler() const;

    bool isFromJsContext(const IJavaScriptContext& jsContext) const;

    const Ref<Context>& getContext() const;

    JSValue getJsValue(IJavaScriptContext& jsContext, JSExceptionTracker& exceptionTracker) const;
    void clearJsValue(IJavaScriptContext& jsContext);

    bool expired() const;

    bool dispose(std::unique_lock<Mutex>& disposablesLock) override;

    const ReferenceInfo& getReferenceInfo() const;
    const StringBox& getANRAttribution() const;

    static Shared<JSValueRefHolder> makeRetainedCallback(IJavaScriptContext& jsContext,
                                                         const JSValue& jsValue,
                                                         const ReferenceInfoBuilder& referenceInfoBuilder,
                                                         JSExceptionTracker& exceptionTracker);

private:
    Ref<Context> _context;
    Weak<JavaScriptTaskScheduler> _taskScheduler;
    JSValueID _jsValueId;
    ReferenceInfo _referenceInfo;
    StringBox _anrAttribution;
    Ref<JSStackTraceProvider> _stackTraceProvider;

    bool doDispose(std::unique_lock<Mutex>& disposablesLock);

    void throwReferenceError(JSExceptionTracker& exceptionTracker, const Error& error) const;
};

} // namespace Valdi
