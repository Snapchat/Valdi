//
//  ViewFactory.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 1/28/20.
//

#pragma once

#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"

#include "valdi/runtime/Views/View.hpp"

#include <deque>

namespace Valdi {

class ViewNodeTree;
class ViewNode;
class ILogger;
class BoundAttributes;
class IViewManager;

class ViewFactory : public ValdiObject {
public:
    ViewFactory(StringBox viewClassName, IViewManager& viewManager, Ref<BoundAttributes> boundAttributes);
    ~ViewFactory() override;

    Ref<View> createView(ViewNodeTree* viewNodeTree, ViewNode* viewNode, bool allowPool);
    void enqueueViewToPool(const Ref<View>& view);
    void clearViewPool();

    const StringBox& getViewClassName() const;
    const Ref<BoundAttributes>& getBoundAttributes() const;
    size_t getPoolSize() const;

    IViewManager& getViewManager() const;

    void setIsUserSpecified(bool isUserSpecified);
    bool isUserSpecified() const;

    void setManagesChildFrames(bool managesChildFrames);
    bool managesChildFrames() const;

    VALDI_CLASS_HEADER(ViewFactory);

protected:
    virtual Ref<View> doCreateView(ViewNodeTree* viewNodeTree, ViewNode* viewNode) = 0;

private:
    StringBox _viewClassName;
    IViewManager& _viewManager;
    Ref<BoundAttributes> _boundAttributes;
    std::deque<Ref<View>> _pooledViews;
    mutable Mutex _mutex;
    bool _isUserSpecified = false;
    bool _managesChildFrames = false;
};

} // namespace Valdi
