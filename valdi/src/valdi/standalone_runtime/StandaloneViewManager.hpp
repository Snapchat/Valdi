//
//  StandaloneViewManager.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/19.
//

#pragma once

#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <vector>

namespace Valdi {

class StandaloneViewManager : public IViewManager {
public:
    StandaloneViewManager();

    Valdi::Ref<Valdi::ViewFactory> createViewFactory(
        const Valdi::StringBox& className, const Valdi::Ref<Valdi::BoundAttributes>& boundAttributes) override;

    void callAction(ViewNodeTree* viewNodeTree,
                    const Valdi::StringBox& actionName,
                    const Ref<ValueArray>& parameters) override;

    PlatformType getPlatformType() const override;
    RenderingBackendType getRenderingBackendType() const override;

    StringBox getDefaultViewClass() override;

    NativeAnimator createAnimator(snap::valdi_core::AnimationType type,
                                  const std::vector<double>& controlPoints,
                                  double duration,
                                  bool beginFromCurrentState,
                                  bool crossfade,
                                  double stiffness,
                                  double damping) override;

    std::vector<StringBox> getClassHierarchy(const StringBox& className) override;

    void bindAttributes(const StringBox& className, Valdi::AttributesBindingContext& binder) override;

    void setRegisterCustomAttributes(bool registerCustomAttributes);
    void setKeepAttributesHistory(bool keepAttributesHistory);
    void setAlwaysRenderInMainThread(bool alwaysRenderInMainThread);
    void setManagesChildFramesForClass(const StringBox& className, bool managesChildFrames);

    void setAllowViewPooling(bool setAllowViewPooling);
    Valdi::Value createViewNodeWrapper(const Valdi::Ref<Valdi::ViewNode>& viewNode,
                                       bool wrapInPlatformReference) override;

    Ref<IViewTransaction> createViewTransaction(const Ref<MainThreadManager>& mainThreadManager,
                                                bool shouldDefer) override;

    Ref<IBitmapFactory> getViewRasterBitmapFactory() const override;

private:
    bool _registerCustomAttributes = false;
    bool _keepAttributesHistory = false;
    bool _allowViewPooling = false;
    bool _alwaysRenderInMainThread = false;
    FlatSet<StringBox> _managedChildFrameClasses;
};

} // namespace Valdi
