//
//  GlobalViewFactories.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 1/28/20.
//

#include "valdi/runtime/Views/GlobalViewFactories.hpp"
#include "valdi/runtime/Attributes/AttributesManager.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Views/ViewFactory.hpp"

#include <iterator>

namespace Valdi {

GlobalViewFactories::GlobalViewFactories(AttributesManager& attributesManager)
    : _attributesManager(attributesManager) {}

GlobalViewFactories::~GlobalViewFactories() = default;

void GlobalViewFactories::clearViewPools() {
    std::vector<Ref<View>> pooledViews;
    takePooledViews(pooledViews);
}

void GlobalViewFactories::takePooledViews(std::vector<Ref<View>>& outViews) const {
    // Destroying a view can be slow (a JNI call per view on Android), so it must not happen under
    // _mutex, which getViewFactory() needs on the JS and preloader threads.
    for (const auto& viewFactory : copyViewFactories()) {
        auto pooledViews = viewFactory->takePooledViews();
        outViews.insert(
            outViews.end(), std::make_move_iterator(pooledViews.begin()), std::make_move_iterator(pooledViews.end()));
    }
}

void GlobalViewFactories::registerViewClassReplacement(const StringBox& className,
                                                       const StringBox& replacementClassName) {
    std::lock_guard<Mutex> guard(_mutex);
    _viewClassReplacements[className] = replacementClassName;

    // Remove the associated view factory for the class we want to replace, so that next queries to
    // getViewFactory will resolve our replacement view factory.
    const auto& it = _viewFactories.find(className);
    if (it != _viewFactories.end()) {
        _viewFactories.erase(it);
    }
}

Ref<ViewFactory> GlobalViewFactories::getViewFactory(const StringBox& className) {
    std::unique_lock<Mutex> guard(_mutex);

    const auto& it = _viewFactories.find(className);
    if (it != _viewFactories.end()) {
        return it->second;
    }

    const auto& replacementIt = _viewClassReplacements.find(className);
    if (replacementIt != _viewClassReplacements.end()) {
        auto replacementClassName = replacementIt->second;
        // Reuse the replacement class' existing factory so both names share a single view pool.
        // Copy the Ref before inserting: FlatMap insertion can invalidate the found iterator.
        auto existingIt = _viewFactories.find(replacementClassName);
        if (existingIt != _viewFactories.end()) {
            auto existingFactory = existingIt->second;
            _viewFactories[className] = existingFactory;
            return existingFactory;
        }
        // Release the mutex during creation, like the non-replacement path below. Factory
        // creation class-loads the view class and binds attributes through JNI; holding _mutex
        // across it blocks clearViewPools() on the main thread during applicationWillPause.
        guard.unlock();
        auto viewFactory = createViewFactory(replacementClassName);
        guard.lock();
        // Another thread may have created a factory under either name while the mutex was
        // released; link to it instead of overwriting, so the view pool doesn't get split.
        auto createdIt = _viewFactories.find(className);
        if (createdIt != _viewFactories.end()) {
            return createdIt->second;
        }
        auto replacementCreatedIt = _viewFactories.find(replacementClassName);
        if (replacementCreatedIt != _viewFactories.end()) {
            auto replacementFactory = replacementCreatedIt->second;
            _viewFactories[className] = replacementFactory;
            return replacementFactory;
        }
        _viewFactories[className] = viewFactory;
        _viewFactories[replacementClassName] = viewFactory;

        return viewFactory;
    } else {
        // Call createViewFactory() with mutex released
        // This is because createViewFactory() calls attributesManager.getAttributesForClass()
        // which could lead the execution back to GlobalViewFactories
        guard.unlock();
        auto viewFactory = createViewFactory(className);
        guard.lock();
        // After re-acquiring the lock, we need to check if another thread has beat us to create
        // the factory and put it in the map
        auto it = _viewFactories.find(className);
        if (it != _viewFactories.end()) {
            return it->second;
        } else {
            _viewFactories[className] = viewFactory;
            return viewFactory;
        }
    }
}

Ref<ViewFactory> GlobalViewFactories::createViewFactory(const StringBox& className) {
    auto boundAttributes = _attributesManager.getAttributesForClass(className);
    return _attributesManager.getViewManager().createViewFactory(className, boundAttributes);
}

std::vector<Ref<ViewFactory>> GlobalViewFactories::copyViewFactories() const {
    std::vector<Ref<ViewFactory>> viewFactories;

    std::lock_guard<Mutex> guard(_mutex);
    viewFactories.reserve(_viewFactories.size());
    for (const auto& it : _viewFactories) {
        viewFactories.emplace_back(it.second);
    }

    return viewFactories;
}

} // namespace Valdi
