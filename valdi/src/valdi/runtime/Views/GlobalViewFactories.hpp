//
//  GlobalViewFactories.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 1/28/20.
//

#pragma once

#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

class AttributesManager;
class View;
class ViewFactory;

class GlobalViewFactories : public SharedPtrRefCountable {
public:
    explicit GlobalViewFactories(AttributesManager& attributesManager);
    ~GlobalViewFactories() override;

    void clearViewPools();
    /**
     * Empties every view pool and appends the pooled views to outViews.
     */
    void takePooledViews(std::vector<Ref<View>>& outViews) const;
    void registerViewClassReplacement(const StringBox& className, const StringBox& replacementClassName);

    Ref<ViewFactory> getViewFactory(const StringBox& className);

    std::vector<Ref<ViewFactory>> copyViewFactories() const;

private:
    AttributesManager& _attributesManager;

    FlatMap<StringBox, StringBox> _viewClassReplacements;
    FlatMap<StringBox, Ref<ViewFactory>> _viewFactories;
    mutable Mutex _mutex;

    Ref<ViewFactory> createViewFactory(const StringBox& className);
};

} // namespace Valdi
