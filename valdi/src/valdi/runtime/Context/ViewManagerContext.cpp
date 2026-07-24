//
//  ViewManagerContext.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 10/19/20.
//

#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Attributes/DefaultAttributeProcessors.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/GlobalViewFactories.hpp"
#include "valdi/runtime/Views/ViewPreloader.hpp"

namespace Valdi {

ViewManagerContext::ViewManagerContext(IViewManager& viewManager,
                                       AttributeIds& attributeIds,
                                       const Ref<ColorPaletteManager>& colorPaletteManager,
                                       const Shared<YGConfig>& yogaConfig,
                                       bool enablePreloading,
                                       const Ref<MainThreadManager>& mainThreadManager,
                                       ILogger& logger)
    : _viewManager(viewManager),
      _attributesManager(viewManager, attributeIds, colorPaletteManager, logger, yogaConfig),
      _mainThreadManager(mainThreadManager) {
    Valdi::registerDefaultProcessors(_attributesManager);

    _globalViewFactories = Valdi::makeShared<GlobalViewFactories>(_attributesManager);

    if (enablePreloading) {
        _viewPreloader = Valdi::makeShared<ViewPreloader>(_globalViewFactories, *mainThreadManager, logger);
    }
}

ViewManagerContext::~ViewManagerContext() {
    if (_viewPreloader != nullptr) {
        _viewPreloader->stopPreload();
    }
}

void ViewManagerContext::registerViewClassReplacement(const StringBox& className,
                                                      const StringBox& replacementClassName) {
    _globalViewFactories->registerViewClassReplacement(className, replacementClassName);
}

void ViewManagerContext::clearViewPools() {
    if (_viewPreloader != nullptr) {
        _viewPreloader->stopPreload();
    }
    _globalViewFactories->clearViewPools();
}

void ViewManagerContext::preloadViews(const StringBox& className, size_t count) {
    if (_viewPreloader != nullptr) {
        _viewPreloader->startPreload(className, count);
    }
}

void ViewManagerContext::setPreloadingPaused(bool preloadingPaused) {
    if (_viewPreloader != nullptr) {
        if (preloadingPaused) {
            _viewPreloader->pausePreload();
        } else {
            _viewPreloader->resumePreload();
        }
    }
}

void ViewManagerContext::setAccessibilityEnabled(const bool accessibilityEnabled) {
    _accessibilityEnabled = accessibilityEnabled;
}

bool ViewManagerContext::getAccessibilityEnabled() const {
    return _accessibilityEnabled;
}

void ViewManagerContext::setPreloadingWorkQueue(const Ref<DispatchQueue>& preloadingWorkQueue) {
    if (_viewPreloader != nullptr) {
        _viewPreloader->setWorkQueue(preloadingWorkQueue);
    }
}

ViewPoolsStats ViewManagerContext::getViewPoolsStats() const {
    ViewPoolsStats stats;

    auto viewFactories = _globalViewFactories->copyViewFactories();
    for (const auto& viewFactory : viewFactories) {
        stats[viewFactory->getViewClassName()] = viewFactory->getPoolSize();
    }

    return stats;
}

} // namespace Valdi
