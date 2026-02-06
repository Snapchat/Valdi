//
//  Context.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Context/Context.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi/runtime/Context/AttributionResolver.hpp"
#include "valdi/runtime/Context/ContextHandler.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Rendering/RenderRequest.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi_core/cpp/Utils/ContainerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include <algorithm>
#include <fmt/format.h>

namespace Valdi {

static void callUpdateCompletedCallbacks(const std::vector<Ref<ValueFunction>>& callbacks) {
    for (const auto& cb : callbacks) {
        (*cb)();
    }
}

Context::PendingRenderRequest::PendingRenderRequest(ContextUpdateId updateId, const Ref<RenderRequest>& renderRequest)
    : updateId(updateId), renderRequest(renderRequest) {};

Context::Context(ContextId contextId, const Ref<ILogger>& logger)
    : Context(contextId,
              AttributionResolver::getDefaultAttribution(),
              nullptr,
              nullptr,
              ComponentPath(),
              nullptr,
              nullptr,
              false,
              true,
              nullptr,
              logger) {}

Context::Context(ContextId contextId,
                 const Attribution& attribution,
                 Ref<ContextHandler> handler,
                 const Ref<ViewManagerContext>& viewManagerContext,
                 const ComponentPath& path,
                 const Shared<ValueConvertible>& viewModel,
                 const Shared<ValueConvertible>& componentContext,
                 bool updateHandlerSynchronously,
                 bool deferRender,
                 Runtime* runtime,
                 const Ref<ILogger>& logger,
                 const StringBox& scopeName)
    : ContextBase(contextId, attribution, path, scopeName),
      _viewModel(viewModel),
      _componentContext(componentContext),
      _handler(std::move(handler)),
      _viewManagerContext(viewManagerContext),
      _logger(makeShared<BridgeLogger>(logger)),
      _runtime(runtime),
      _disposables(_mutex),
      _updateHandlerSynchronously(updateHandlerSynchronously),
      _deferRender(deferRender) {
    _creationWatch.start();
}

Context::~Context() = default;

void Context::postInit() {
    _logger->setListener(weakRef(this));
    retainDisposables();
}

void Context::onCreate() {
    Ref<ContextHandler> handler;
    auto notifySync = false;
    {
        std::lock_guard<Mutex> lock(_mutex);
        SC_ASSERT(!_created);
        _created = true;
        handler = _handler;
        notifySync = _updateHandlerSynchronously;
    }

    if (handler != nullptr) {
        handler->onCreate(strongSmallRef(this), _viewModel, _componentContext, enqueueUpdate(), notifySync);
    }
}

void Context::onDestroy() {
    Ref<ContextHandler> handler;
    std::vector<Ref<ValueFunction>> updateCompletedCallbacks;
    std::deque<PendingRenderRequest> pendingRenderRequests;
    Ref<Metrics> metrics;
    auto notifySync = false;

    {
        std::unique_lock<Mutex> lock(_mutex);
        if (_destroyed) {
            return;
        }

        _destroyed = true;
        _componentContext = nullptr;
        handler = std::move(_handler);
        _viewModel = nullptr;
        if (_runtime != nullptr) {
            metrics = _runtime->getMetrics();
            _runtime = nullptr;
        }
        updateCompletedCallbacks = std::move(_updateCompletedCallbacks);
        // NOTE(rjaber): The pending render request can contain a disposable, which will request access to the lock
        pendingRenderRequests = std::move(_pendingRenderRequests);
        notifySync = _updateHandlerSynchronously;
    }

    if (handler != nullptr) {
        auto sessionTime = _creationWatch.elapsed();
        if (metrics != nullptr) {
            metrics->emitSessionTime(getPath().getResourceId().bundleName, sessionTime);
        }

        handler->onDestroy(strongSmallRef(this), notifySync);
    }

    callUpdateCompletedCallbacks(updateCompletedCallbacks);
    releaseDisposables();
}

void Context::onRendered() {
    if (_didPerformInitialRender) {
        return;
    }
    _didPerformInitialRender = true;

    auto initialRenderLatency = _creationWatch.elapsed();
    Ref<Metrics> metrics = _runtime != nullptr ? _runtime->getMetrics() : nullptr;
    if (metrics != nullptr) {
        metrics->emitInitialRenderLatency(getPath().getResourceId().bundleName, initialRenderLatency);
    }
}

void Context::onAttributeChange(RawViewNodeId viewNodeId,
                                const StringBox& attributeName,
                                const Value& attributeValue,
                                bool shouldNotifySync) {
    Ref<ContextHandler> handler;
    auto notifySync = false;

    {
        std::lock_guard<Mutex> guard(_mutex);
        if (_handler == nullptr) {
            return;
        }
        handler = _handler;
        notifySync = _updateHandlerSynchronously;
    }

    handler->onAttributeChange(
        strongSmallRef(this), viewNodeId, attributeName, attributeValue, shouldNotifySync || notifySync);
}

void Context::onRuntimeIssue(const StringBox& message, bool isError) {
    Ref<ContextHandler> handler;
    auto notifySync = false;

    {
        std::lock_guard<Mutex> guard(_mutex);
        if (_handler == nullptr) {
            return;
        }
        handler = _handler;
        notifySync = _updateHandlerSynchronously;
    }

    handler->onRuntimeIssue(strongSmallRef(this), isError, message, notifySync);
}

bool Context::isDestroyed() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _destroyed;
}

void Context::setViewModel(const Shared<ValueConvertible>& viewModel) {
    Ref<ContextHandler> handler;
    ContextUpdateId updateId = 0;
    auto notifySync = false;

    {
        std::lock_guard<Mutex> guard(_mutex);
        _viewModel = viewModel;
        if (_handler == nullptr) {
            return;
        }
        handler = _handler;
        updateId = lockFreeEnqueueUpdate();
        notifySync = _updateHandlerSynchronously;
    }

    handler->onViewModelUpdate(strongSmallRef(this), viewModel, updateId, notifySync);
}

Ref<ContextHandler> Context::getHandler() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _handler;
}

Shared<ValueConvertible> Context::getViewModel() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _viewModel;
}

void Context::markUpdateCompleted(ContextUpdateId updateId, std::unique_lock<Mutex>& guard) {
    std::vector<Ref<ValueFunction>> updateCompletedCallbacks;

    if (!guard.owns_lock()) {
        guard.lock();
    }

    auto it = _pendingUpdates.find(updateId);
    if (it != _pendingUpdates.end()) {
        _pendingUpdates.erase(it);
        if (_pendingUpdates.empty()) {
            std::swap(updateCompletedCallbacks, _updateCompletedCallbacks);
        }
    }

    guard.unlock();

    callUpdateCompletedCallbacks(updateCompletedCallbacks);
}

ContextUpdateId Context::lockFreeEnqueueUpdate() {
    if (_destroyed) {
        return 0;
    }

    auto updateId = ++_updateSequence;
    _pendingUpdates.insert(updateId);
    return updateId;
}

ContextUpdateId Context::enqueueUpdate() {
    std::lock_guard<Mutex> guard(_mutex);
    return lockFreeEnqueueUpdate();
}

std::optional<ContextUpdateId> Context::enqueueRenderRequest(const Ref<RenderRequest>& renderRequest) {
    std::unique_lock<Mutex> guard(_mutex);
    if (_destroyed) {
        return std::optional<ContextUpdateId>();
    }

    auto updateId = lockFreeEnqueueUpdate();
    _pendingRenderRequests.emplace_back(updateId, renderRequest);

    if (!_deferRender) {
        runNextPendingRenderRequest(guard);
        return std::optional<ContextUpdateId>();
    }

    return updateId;
}

bool Context::runRenderRequest(ContextUpdateId updateId) {
    std::unique_lock<Mutex> guard(_mutex);
    if (_destroyed || _pendingRenderRequests.empty() || updateId != _pendingRenderRequests.front().updateId) {
        return false;
    }
    runNextPendingRenderRequest(guard);
    return true;
}

void Context::runNextPendingRenderRequest(std::unique_lock<Mutex>& guard) {
    auto pendingRenderRequest = _pendingRenderRequests.front();
    _pendingRenderRequests.pop_front();
    Ref<Runtime> runtime(_runtime);
    guard.unlock();
    runtime->processRenderRequest(pendingRenderRequest.renderRequest);
    markUpdateCompleted(pendingRenderRequest.updateId, guard);
}

bool Context::flushRenderRequests() {
    bool didFlush = false;
    std::unique_lock<Mutex> guard(_mutex);
    while (!_pendingRenderRequests.empty()) {
        runNextPendingRenderRequest(guard);
        guard.lock();
        didFlush = true;
    }

    return didFlush;
}

bool Context::hasPendingRenderRequests() const {
    std::lock_guard<Mutex> guard(_mutex);
    return !_pendingRenderRequests.empty();
}

void Context::markUpdateCompleted(ContextUpdateId updateId) {
    std::unique_lock<Mutex> guard(_mutex);
    markUpdateCompleted(updateId, guard);
}

void Context::waitUntilAllUpdatesCompleted(const Ref<ValueFunction>& callback) {
    auto enqueued = false;
    {
        std::lock_guard<Mutex> guard(_mutex);
        if (!_destroyed && !_pendingUpdates.empty()) {
            _updateCompletedCallbacks.emplace_back(callback);
            enqueued = true;
        }
    }

    if (!enqueued) {
        (*callback)();
    }
}

void Context::waitUntilAllUpdatesCompletedSync(bool shouldFlushRenderRequests) {
    auto group = Valdi::makeShared<AsyncGroup>();
    group->enter();

    waitUntilAllUpdatesCompleted(makeShared<ValueFunctionWithCallable>([group](const auto& /*callContext*/) -> Value {
        group->leave();
        return Value();
    }));

    std::unique_lock<Mutex> lock(_mutex);
    if (_destroyed) {
        return;
    }

    Ref<Runtime> runtime(_runtime);
    if (runtime->getMainThreadManager().currentThreadIsMainThread()) {
        auto& mainThreadManager = runtime->getMainThreadManager();
        while (!_destroyed && !group->isCompleted()) {
            lock.unlock();
            if (!mainThreadManager.runNextTask()) {
                // Release the thread and the lock for a bit
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
            lock.lock();
        }
    } else {
        lock.unlock();
        if (shouldFlushRenderRequests) {
            while (!group->isCompleted()) {
                // Release the thread for a bit.
                // This code and the main thread handling code above
                // could be refactored to use a notification system instead,
                // so that we wouldn't have to wait arbitrarily.
                if (!flushRenderRequests()) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            }
        } else {
            group->blockingWait();
        }
    }
}

Runtime* Context::getRuntime() const {
    return _runtime;
}

const Shared<ValueConvertible>& Context::getComponentContext() const {
    return _componentContext;
}

void Context::setUserData(const Ref<ValdiObject>& userData) {
    std::lock_guard<Mutex> guard(_mutex);
    _userData = userData;
}

bool Context::insertDisposable(Disposable* disposable) {
    return _disposables.insert(disposable);
}

bool Context::removeDisposable(Disposable* disposable) {
    return _disposables.remove(disposable);
}

std::vector<Disposable*> Context::getAllDisposables() const {
    return _disposables.all();
}

void Context::setDisposableListener(const Ref<DisposableGroupListener>& disposableGroupListener) {
    _disposables.setListener(disposableGroupListener);
}

void Context::retainDisposables() {
    _disposableRefCount++;
}

void Context::releaseDisposables() {
    auto value = --_disposableRefCount;
    if (value == 0) {
        std::unique_lock<Mutex> guard(_mutex);
        _disposables.dispose(guard);
    }
}

void Context::setParent(const Ref<Context>& parentContext) {
    _parentContext = parentContext;
}

const Ref<Context>& Context::getParent() const {
    return _parentContext;
}

Context* Context::getRoot() {
    auto* current = this;
    while (current != nullptr && current->getParent() != nullptr) {
        current = current->getParent().get();
    }

    return current;
}

std::unique_lock<Mutex> Context::lock() const {
    return std::unique_lock<Mutex>(_mutex);
}

Ref<ValdiObject> Context::getUserData() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _userData;
}

ILogger& Context::getLogger() {
    return *_logger;
}

void Context::willLog(LogType type, const std::string& message) {
    if (type < LogTypeWarn) {
        return;
    }
    onRuntimeIssue(StringCache::getGlobal().makeString(message), type >= LogTypeError);
}

void Context::setUpdateHandlerSynchronously(bool updateHandlerSynchronously) {
    std::lock_guard<Mutex> guard(_mutex);
    _updateHandlerSynchronously = updateHandlerSynchronously;
}

void Context::setCurrent(const Ref<Context>& currentContext) {
    ContextBase::setCurrent(currentContext.get());
}

Ref<Context> Context::currentRef() {
    return Ref<Context>(Context::current());
}

Context* Context::current() {
    return dynamic_cast<Context*>(ContextBase::current());
}

Context* Context::currentRoot() {
    auto current = Context::current();
    while (current != nullptr && current->getParent() != nullptr) {
        current = current->getParent().get();
    }

    return current;
}

const Ref<ViewManagerContext>& Context::getViewManagerContext() const {
    return _viewManagerContext;
}

RetainedContext::RetainedContext(const Ref<Context>& context) : context(context) {
    retain();
}

RetainedContext::RetainedContext(Ref<Context>&& context) : context(std::move(context)) {
    retain();
}

RetainedContext::RetainedContext(const RetainedContext& other) : context(other.context) {
    retain();
}

RetainedContext::RetainedContext(RetainedContext&& other) noexcept : context(std::move(other.context)) {}

RetainedContext::~RetainedContext() {
    release();
}

RetainedContext& RetainedContext::operator=(const RetainedContext& other) {
    if (this != &other) {
        release();
        context = other.context;
        retain();
    }
    return *this;
}

RetainedContext& RetainedContext::operator=(RetainedContext&& other) noexcept {
    if (this != &other) {
        release();
        context = std::move(other.context);
    }
    return *this;
}

void RetainedContext::retain() const {
    if (context != nullptr) {
        context->retainDisposables();
    }
}

void RetainedContext::release() const {
    if (context != nullptr) {
        context->releaseDisposables();
    }
}

} // namespace Valdi
