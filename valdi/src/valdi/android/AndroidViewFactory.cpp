//
//  NativeBridge.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/9/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"

#include "valdi_core/jni/JavaCache.hpp"

#include "valdi/android/AndroidViewFactory.hpp"
#include "valdi/android/AndroidViewHolder.hpp"
#include "valdi/android/ViewManager.hpp"

namespace ValdiAndroid {

AndroidViewFactory::AndroidViewFactory(GlobalRefJavaObjectBase viewFactory,
                                       Valdi::StringBox viewClassName,
                                       ViewManager& viewManager,
                                       Valdi::Ref<Valdi::BoundAttributes> boundAttributes,
                                       bool isUserSpecified)
    : Valdi::ViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)),
      _viewManager(viewManager),
      _viewFactory(std::move(viewFactory)) {
    setIsUserSpecified(isUserSpecified);
    setManagesChildFrames(
        ValdiAndroid::JavaEnv::getCache().getViewFactoryManagesChildFramesMethod().call(_viewFactory.toObject()));
}

AndroidViewFactory::~AndroidViewFactory() = default;

Valdi::Ref<Valdi::View> AndroidViewFactory::doCreateView(Valdi::ViewNodeTree* viewNodeTree,
                                                         Valdi::ViewNode* /*viewNode*/) {
    ValdiContext javaValdiContext(_viewFactory.getEnv(), nullptr);

    if (viewNodeTree != nullptr) {
        javaValdiContext = fromValdiContextUserData(viewNodeTree->getContext()->getUserData());
    }

    auto result = ValdiAndroid::JavaEnv::getCache().getViewFactoryCreateViewMethod().call(_viewFactory.toObject(),
                                                                                          javaValdiContext);
    if (result.isNull()) {
        return nullptr;
    }
    return ValdiAndroid::toValdiView(result, _viewManager);
}

} // namespace ValdiAndroid
