//
//  ViewManagerContext.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 10/19/20.
//

#pragma once

#include "valdi/runtime/Attributes/AttributesManager.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include "valdi_core/Platform.hpp"

namespace Valdi {

class IViewManager;
class GlobalViewFactories;
class DispatchQueue;
class ViewPreloader;
class MainThreadManager;
class ColorPaletteManager;

using ViewPoolsStats = FlatMap<StringBox, size_t>;

class ViewManagerContext : public SimpleRefCountable {
public:
    ViewManagerContext(IViewManager& viewManager,
                       AttributeIds& attributeIds,
                       const Ref<ColorPaletteManager>& colorPaletteManager,
                       const Shared<YGConfig>& yogaConfig,
                       bool enablePreloading,
                       const Ref<MainThreadManager>& mainThreadManager,
                       ILogger& logger);
    ~ViewManagerContext() override;

    constexpr IViewManager& getViewManager() const {
        return _viewManager;
    }

    constexpr const AttributesManager& getAttributesManager() const {
        return _attributesManager;
    }

    constexpr AttributesManager& getAttributesManager() {
        return _attributesManager;
    }

    constexpr const Ref<GlobalViewFactories>& getGlobalViewFactories() const {
        return _globalViewFactories;
    }

    constexpr const Ref<MainThreadManager>& getMainThreadManager() const {
        return _mainThreadManager;
    }

    void clearViewPools();

    /**
     Register a replacement for a view class.
     */
    void registerViewClassReplacement(const StringBox& className, const StringBox& replacementClassName);

    void preloadViews(const StringBox& className, size_t count);
    void setPreloadingPaused(bool preloadingPaused);

    void setPreloadingWorkQueue(const Ref<DispatchQueue>& preloadingWorkQueue);

    void setAccessibilityEnabled(const bool accessibilityEnabled);
    bool getAccessibilityEnabled() const;

    ViewPoolsStats getViewPoolsStats() const;

private:
    IViewManager& _viewManager;
    AttributesManager _attributesManager;
    Ref<GlobalViewFactories> _globalViewFactories;
    Ref<ViewPreloader> _viewPreloader;
    Ref<MainThreadManager> _mainThreadManager;
    bool _accessibilityEnabled;
};

} // namespace Valdi
