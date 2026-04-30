//
//  JSValueRefHolder.cpp
//  valdi
//
//  Created by Simon Corsin on 5/11/21.
//

#include "valdi/runtime/JavaScript/JSValueRefHolder.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/ErrorCodes.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"

namespace Valdi {

JSValueRefHolder::JSValueRefHolder(IJavaScriptContext& jsContext,
                                   const JSValue& jsValue,
                                   const ReferenceInfo& referenceInfo,
                                   JSExceptionTracker& exceptionTracker,
                                   bool captureStackTrace)
    : _context(Context::currentRoot()),
      _taskScheduler(weakRef(jsContext.getTaskScheduler())),
      _jsValueId(jsContext.stashJSValue(jsValue)),
      _referenceInfo(referenceInfo) {
    SC_ASSERT(_context != nullptr, "No context set when creating the JSValueRefHolder");
    _context->insertDisposable(this);

    if (captureStackTrace) {
        if (exceptionTracker.getStackTraceProvider() == nullptr && jsContext.getListener() != nullptr) {
            exceptionTracker.setStackTraceProvider(jsContext.getListener()->captureCurrentStackTrace());
        }

        _stackTraceProvider = exceptionTracker.getStackTraceProvider();
    }
}

// JSValueRefHolder deallocation triggers a tsan warning, as it is possible that
// the dispose() virtual method gets called while the holder is being destroyed.
// This is not a problem in practice as we acquire the lock during the removeDisposable()
// call, so the destructor will end up "pausing" while doDispose() is called
__attribute__((no_sanitize("thread"))) JSValueRefHolder::~JSValueRefHolder() {
    _context->removeDisposable(this);
    auto lock = _context->lock();
    doDispose(lock);
}

bool JSValueRefHolder::expired() const {
    auto taskScheduler = _taskScheduler.lock();
    return taskScheduler == nullptr || taskScheduler->isDisposed();
}

Shared<JavaScriptTaskScheduler> JSValueRefHolder::getTaskScheduler() const {
    return _taskScheduler.lock();
}

const Ref<Context>& JSValueRefHolder::getContext() const {
    return _context;
}

bool JSValueRefHolder::isFromJsContext(const IJavaScriptContext& jsContext) const {
    return getTaskScheduler().get() == jsContext.getTaskScheduler();
}

const ReferenceInfo& JSValueRefHolder::getReferenceInfo() const {
    return _referenceInfo;
}

void JSValueRefHolder::throwReferenceError(JSExceptionTracker& exceptionTracker, const Error& error) const {
    if (_stackTraceProvider != nullptr) {
        auto stack = _stackTraceProvider->getStackTrace();
        std::string errorMessage = error.toString();
        exceptionTracker.onError(Error(StringCache::getGlobal().makeString(std::move(errorMessage)), stack, nullptr));
    } else {
        exceptionTracker.onError(error);
    }
}

JSValue JSValueRefHolder::getJsValue(IJavaScriptContext& jsContext, JSExceptionTracker& exceptionTracker) const {
    auto jsValue = jsContext.getStashedJSValue(_jsValueId);
    if (!jsValue) {
        auto errorMessage = STRING_FORMAT(
            "Cannot unwrap JS value reference '{}' as it was disposed. Reference was taken from context {}",
            _referenceInfo,
            _context->getIdAndPathString());
        throwReferenceError(exceptionTracker, Error(errorMessage, ErrorCodes::Composer::CANNOT_UNWRAP_DISPOSED_JS_VALUE));
        return JSValue();
    }

    return jsValue.value();
}

void JSValueRefHolder::clearJsValue(IJavaScriptContext& jsContext) {
    auto lock = _context->lock();
    auto jsValueId = _jsValueId;

    if (jsValueId.isNull()) {
        return;
    }

    _jsValueId = JSValueID();
    jsContext.removedStashedJSValue(jsValueId);
}

bool JSValueRefHolder::dispose(std::unique_lock<Mutex>& disposablesLock) {
    return doDispose(disposablesLock);
}

static void doReleaseJsValue(const Shared<JavaScriptTaskScheduler>& taskScheduler,
                             const Ref<Context>& context,
                             const JSValueID& jsValueId) {
    taskScheduler->dispatchOnJsThreadAsync(context, [jsValueId](JavaScriptEntryParameters& jsEntry) {
        jsEntry.jsContext.removedStashedJSValue(jsValueId);
    });
}

bool JSValueRefHolder::doDispose(std::unique_lock<Mutex>& disposablesLock) {
    auto jsValueId = _jsValueId;

    if (jsValueId.isNull()) {
        return false;
    }

    _jsValueId = JSValueID();

    auto taskScheduler = _taskScheduler.lock();

    if (taskScheduler != nullptr) {
        auto context = _context;

        disposablesLock.unlock();

        doReleaseJsValue(taskScheduler, context, jsValueId);
    }

    return true;
}

Shared<JSValueRefHolder> JSValueRefHolder::makeRetainedCallback(IJavaScriptContext& jsContext,
                                                                const JSValue& jsValue,
                                                                const ReferenceInfoBuilder& referenceInfoBuilder,
                                                                JSExceptionTracker& exceptionTracker) {
    return makeShared<JSValueRefHolder>(
        jsContext, jsValue, referenceInfoBuilder.asFunction().build(), exceptionTracker, true);
}

} // namespace Valdi
