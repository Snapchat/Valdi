//
//  ContextManager.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Resources/Bundle.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi_core/cpp/Context/ComponentPath.hpp"
#include "valdi_core/cpp/Resources/ResourceId.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace Valdi {

class ILogger;
class ViewManagerContext;
class AttributionResolver;

class IContextManagerListener {
public:
    IContextManagerListener() = default;
    virtual ~IContextManagerListener() = default;

    virtual void onContextCreated(const SharedContext& context) = 0;
    virtual void onContextDestroyed(Context& context) = 0;
};

/**
 * Can create a Context from a loaded document and associate them with an id. It holds the shared pointers for all
 * contexts. The shared pointers get released when DestroyContext() is called.
 */
class ContextManager {
public:
    ContextManager(const Ref<ILogger>& logger, Runtime* runtime);
    ~ContextManager();

    SharedContext createContext(Ref<ContextHandler> handler,
                                const Ref<ViewManagerContext>& viewManagerContext,
                                bool deferRender,
                                const StringBox& scopeName = StringBox());

    SharedContext createContext(Ref<ContextHandler> handler,
                                const Ref<ViewManagerContext>& viewManagerContext,
                                const ComponentPath& path,
                                const Shared<ValueConvertible>& viewModel,
                                const Shared<ValueConvertible>& componentContext,
                                bool updateHandlerSynchronously,
                                bool deferRender,
                                const StringBox& scopeName = StringBox());
    void destroyContext(const SharedContext& context);
    bool destroyContext(ContextId contextId);

    SharedContext getContext(ContextId contextId) const;

    void destroyAllContexts();

    std::vector<SharedContext> getAllContexts() const;
    size_t getContextsSize() const;

    void setListener(IContextManagerListener* listener);

    ContextUpdateId enqueueUpdateForContext(ContextId contextId) const;

    void setAttributionResolver(const Ref<AttributionResolver>& attributionResolver);

private:
    ContextId _contextIdSequence = 0;
    FlatMap<ContextId, SharedContext> _contextById;
    mutable std::mutex _mutex;
    Ref<ILogger> _logger;
    Runtime* _runtime;
    Ref<AttributionResolver> _attributionResolver;
    IContextManagerListener* _listener = nullptr;

    SharedContext lockFreeGetContext(ContextId contextId) const;
};

} // namespace Valdi
