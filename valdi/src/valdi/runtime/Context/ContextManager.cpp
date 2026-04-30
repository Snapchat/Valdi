//
//  ContextManager.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Context/ContextManager.hpp"
#include "valdi/runtime/Context/AttributionResolver.hpp"
#include "valdi/runtime/Context/ContextHandler.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Resources/ResourceId.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <algorithm>
#include <fmt/format.h>

namespace Valdi {

ContextManager::ContextManager(const Ref<ILogger>& logger, Runtime* runtime) : _logger(logger), _runtime(runtime) {}
ContextManager::~ContextManager() = default;

SharedContext ContextManager::createContext(Ref<ContextHandler> handler, // NOLINT(performance-unnecessary-value-param)
                                            const Ref<ViewManagerContext>& viewManagerContext,
                                            bool deferRender,
                                            const StringBox& scopeName) {
    return createContext(std::move(handler), viewManagerContext, ComponentPath(), nullptr, nullptr, false, deferRender, scopeName);
}

SharedContext ContextManager::createContext(Ref<ContextHandler> handler, // NOLINT(performance-unnecessary-value-param)
                                            const Ref<ViewManagerContext>& viewManagerContext,
                                            const ComponentPath& path,
                                            const Shared<ValueConvertible>& viewModel,
                                            const Shared<ValueConvertible>& componentContext,
                                            bool updateHandlerSynchronously,
                                            bool deferRender,
                                            const StringBox& scopeName) {
    Ref<Context> context;
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto contextId = ++_contextIdSequence;
        auto attribution = _attributionResolver != nullptr ?
                               _attributionResolver->resolveAttribution(path.getResourceId().bundleName) :
                               AttributionResolver::getDefaultAttribution();
        context = Valdi::makeShared<Context>(contextId,
                                             attribution,
                                             std::move(handler),
                                             viewManagerContext,
                                             path,
                                             viewModel,
                                             componentContext,
                                             updateHandlerSynchronously,
                                             deferRender,
                                             _runtime,
                                             _logger,
                                             scopeName);
        context->postInit();

        _contextById[contextId] = context;
    }
    if (Valdi::traceInitialization) {
        if (!path.isEmpty()) {
            VALDI_INFO(*_logger,
                       "Creating Valdi Context with id {} with component path '{}'",
                       context->getContextId(),
                       path.toString());
        } else if (!scopeName.isEmpty()) {
            VALDI_INFO(*_logger,
                       "Creating Valdi Context with id {} in scope '{}'",
                       context->getContextId(),
                       scopeName.slowToString());
        } else {
            VALDI_INFO(*_logger, "Creating Valdi Context with id {}", context->getContextId());
        }
    }

    if (_listener != nullptr) {
        _listener->onContextCreated(context);
    }

    return context;
}

void ContextManager::destroyContext(const SharedContext& context) {
    destroyContext(context->getContextId());
}

bool ContextManager::destroyContext(ContextId contextId) {
    Ref<Context> context;
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _contextById.find(contextId);
        if (it == _contextById.end()) {
            return false;
        }
        context = std::move(it->second);
        _contextById.erase(it);
    }

    if (Valdi::traceInitialization) {
        VALDI_INFO(*_logger, "Destroying Valdi Context with id {}", context->getContextId());
    }

    context->onDestroy();

    if (_listener != nullptr) {
        _listener->onContextDestroyed(*context);
    }

    return true;
}

void ContextManager::destroyAllContexts() {
    auto contexts = getAllContexts();
    for (const auto& context : contexts) {
        destroyContext(context);
    }
}

SharedContext ContextManager::lockFreeGetContext(ContextId contextId) const {
    auto it = _contextById.find(contextId);

    if (it == _contextById.end()) {
        return nullptr;
    }

    return it->second;
}

SharedContext ContextManager::getContext(ContextId contextId) const {
    std::lock_guard<std::mutex> lock(_mutex);

    return lockFreeGetContext(contextId);
}

std::vector<SharedContext> ContextManager::getAllContexts() const {
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<SharedContext> out;

    for (const auto& it : _contextById) {
        out.push_back(it.second);
    }

    return out;
}

size_t ContextManager::getContextsSize() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _contextById.size();
}

void ContextManager::setListener(IContextManagerListener* listener) {
    _listener = listener;
}

void ContextManager::setAttributionResolver(const Ref<AttributionResolver>& attributionResolver) {
    std::lock_guard<std::mutex> lock(_mutex);
    _attributionResolver = attributionResolver;
}

} // namespace Valdi
