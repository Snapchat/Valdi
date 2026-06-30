//
//  ViewFactory.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 1/28/20.
//

#include "valdi/runtime/Views/ViewFactory.hpp"
#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace Valdi {

ViewFactory::ViewFactory(StringBox viewClassName, IViewManager& viewManager, Ref<BoundAttributes> boundAttributes)
    : _viewClassName(std::move(viewClassName)),
      _viewManager(viewManager),
      _boundAttributes(std::move(boundAttributes)) {}

ViewFactory::~ViewFactory() = default;

void ViewFactory::clearViewPool() {
    std::lock_guard<Mutex> lock(_mutex);
    _pooledViews.clear();
}

Ref<View> ViewFactory::createView(ViewNodeTree* viewNodeTree, ViewNode* viewNode, bool allowPool) {
    if (allowPool) {
        std::lock_guard<Mutex> lock(_mutex);

        if (!_pooledViews.empty()) {
            auto view = _pooledViews.front();
            _pooledViews.pop_front();

            return view;
        }
    }

    VALDI_TRACE_META("Valdi.createView", _viewClassName);
    return doCreateView(viewNodeTree, viewNode);
}

void ViewFactory::enqueueViewToPool(const Ref<View>& view) {
    std::lock_guard<Mutex> lock(_mutex);
    _pooledViews.emplace_back(view);
}

const StringBox& ViewFactory::getViewClassName() const {
    return _viewClassName;
}

const Ref<BoundAttributes>& ViewFactory::getBoundAttributes() const {
    return _boundAttributes;
}

size_t ViewFactory::getPoolSize() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _pooledViews.size();
}

IViewManager& ViewFactory::getViewManager() const {
    return _viewManager;
}

void ViewFactory::setIsUserSpecified(bool isUserSpecified) {
    _isUserSpecified = isUserSpecified;
}

bool ViewFactory::isUserSpecified() const {
    return _isUserSpecified;
}

void ViewFactory::setManagesChildFrames(bool managesChildFrames) {
    _managesChildFrames = managesChildFrames;
}

bool ViewFactory::managesChildFrames() const {
    return _managesChildFrames;
}

VALDI_CLASS_IMPL(ViewFactory);

} // namespace Valdi
