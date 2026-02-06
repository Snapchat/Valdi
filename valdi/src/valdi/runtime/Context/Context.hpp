//
//  Context.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Context/ContextBase.hpp"

#include "valdi/runtime/Context/RawViewNodeId.hpp"
#include "valdi/runtime/Utils/BridgeLogger.hpp"
#include "valdi/runtime/Utils/DisposableGroup.hpp"
#include "valdi_core/cpp/Context/ComponentPath.hpp"
#include "valdi_core/cpp/Threading/TaskQueue.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueConvertible.hpp"

#include "utils/time/StopWatch.hpp"

#include <deque>
#include <memory>
#include <mutex>

namespace Valdi {

class Context;
using SharedContext = Ref<Context>;

class ContextHandler;
class ContextComponentRenderer;
class ViewManagerContext;

class Runtime;
class RenderRequest;

struct RetainedContext {
    Ref<Context> context;

    RetainedContext(const Ref<Context>& context);
    RetainedContext(Ref<Context>&& context);

    RetainedContext(const RetainedContext& other);
    RetainedContext(RetainedContext&& other) noexcept;
    ~RetainedContext();

    RetainedContext& operator=(const RetainedContext& other);
    RetainedContext& operator=(RetainedContext&& other) noexcept;

private:
    void retain() const;
    void release() const;
};

/**
 * Association of a View tree with a Valdi document.
 * At first the view tree will be empty, until it has been rendered.
 * The context has a unique id and holds all the views.
 * It can have children contexts. This happens when a valdi document
 * has an element that refers to another valdi document.
 */
class Context : public ContextBase, public BridgeLoggerListener {
public:
    Context(ContextId contextId, const Ref<ILogger>& logger);

    Context(ContextId contextId,
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
            const StringBox& scopeName = StringBox());
    ~Context() override;

    void postInit();

    void onCreate();
    void onDestroy();
    void onRendered();
    void onAttributeChange(RawViewNodeId viewNodeId,
                           const StringBox& attributeName,
                           const Value& attributeValue,
                           bool shouldNotifySync);
    void onRuntimeIssue(const StringBox& message, bool isError);

    bool isDestroyed() const;

    Shared<ValueConvertible> getViewModel() const;
    void setViewModel(const Shared<ValueConvertible>& viewModel);

    const Shared<ValueConvertible>& getComponentContext() const;

    Ref<ContextHandler> getHandler() const;

    Ref<ValdiObject> getUserData() const;
    void setUserData(const Ref<ValdiObject>& userData);

    ILogger& getLogger();

    const Ref<ViewManagerContext>& getViewManagerContext() const;

    Runtime* getRuntime() const;

    void markUpdateCompleted(ContextUpdateId updateId);
    ContextUpdateId enqueueUpdate();

    std::optional<ContextUpdateId> enqueueRenderRequest(const Ref<RenderRequest>& renderRequest);
    bool runRenderRequest(ContextUpdateId updateId);
    bool flushRenderRequests();
    bool hasPendingRenderRequests() const;

    void waitUntilAllUpdatesCompleted(const Ref<ValueFunction>& callback);
    void waitUntilAllUpdatesCompletedSync(bool shouldFlushRenderRequests);

    void setParent(const Ref<Context>& parentContext);
    const Ref<Context>& getParent() const;
    Context* getRoot();

    bool insertDisposable(Disposable* disposable);
    bool removeDisposable(Disposable* disposable);
    std::vector<Disposable*> getAllDisposables() const;
    void setDisposableListener(const Ref<DisposableGroupListener>& disposableGroupListener);

    void setUpdateHandlerSynchronously(bool updateHandlerSynchronously);

    void retainDisposables();
    void releaseDisposables();

    /**
     Acquires the underlying lock for this Context
     */
    std::unique_lock<Mutex> lock() const;

    static Ref<Context> currentRef();
    static Context* current();
    static Context* currentRoot();
    static void setCurrent(const Ref<Context>& currentContext);

protected:
    void willLog(LogType type, const std::string& message) override;

private:
    struct PendingRenderRequest {
        PendingRenderRequest(ContextUpdateId updateId, const Ref<RenderRequest>& renderRequest);

        ContextUpdateId updateId;
        Ref<RenderRequest> renderRequest;
    };

    mutable Mutex _mutex;

    ContextUpdateId _updateSequence = 0;
    Shared<ValueConvertible> _viewModel;
    Shared<ValueConvertible> _componentContext;
    Ref<ContextHandler> _handler;
    Ref<ViewManagerContext> _viewManagerContext;
    Ref<ValdiObject> _userData;
    Ref<BridgeLogger> _logger;
    snap::utils::time::StopWatch _creationWatch;
    Runtime* _runtime;
    DisposableGroup _disposables;
    FlatSet<ContextUpdateId> _pendingUpdates;
    std::vector<Ref<ValueFunction>> _updateCompletedCallbacks;
    Ref<Context> _parentContext;
    std::deque<PendingRenderRequest> _pendingRenderRequests;

    bool _created = false;
    bool _destroyed = false;
    bool _didPerformInitialRender = false;
    bool _updateHandlerSynchronously = false;
    bool _deferRender = false;
    std::atomic_int _disposableRefCount = 0;

    ContextUpdateId lockFreeEnqueueUpdate();
    void markUpdateCompleted(ContextUpdateId updateId, std::unique_lock<Mutex>& guard);

    void runNextPendingRenderRequest(std::unique_lock<Mutex>& guard);
};

} // namespace Valdi
