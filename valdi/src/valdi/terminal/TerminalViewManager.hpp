#pragma once

#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/terminal/Classes/ITerminalViewClass.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

#include <vector>

namespace Valdi {

class ILogger;

/**
 * IViewManager for the terminal backend. Views are TerminalViews laid out in whole cells and
 * painted by their view class into a cell grid. Classes are registered under both their iOS and
 * Android names so components resolve the same way on either platform.
 */
class TerminalViewManager : public IViewManager {
public:
    explicit TerminalViewManager(PlatformType platformType = PlatformTypeIOS);
    TerminalViewManager(PlatformType platformType, ILogger& logger);
    ~TerminalViewManager() override;

    /** Registers a view class, replacing any class registered under the same names. */
    void registerClass(const Ref<ITerminalViewClass>& viewClass);

    /** The class registered under `className`, or nullptr. */
    Ref<ITerminalViewClass> getViewClass(const StringBox& className) const;

    PlatformType getPlatformType() const override;
    RenderingBackendType getRenderingBackendType() const override;

    StringBox getDefaultViewClass() override;

    Ref<ViewFactory> createViewFactory(const StringBox& className,
                                       const Ref<BoundAttributes>& boundAttributes) override;

    void callAction(ViewNodeTree* viewNodeTree,
                    const StringBox& actionName,
                    const Ref<ValueArray>& parameters) override;

    NativeAnimator createAnimator(snap::valdi_core::AnimationType type,
                                  const std::vector<double>& controlPoints,
                                  double duration,
                                  bool beginFromCurrentState,
                                  bool crossfade,
                                  double stiffness,
                                  double damping) override;

    std::vector<StringBox> getClassHierarchy(const StringBox& className) override;

    void bindAttributes(const StringBox& className, AttributesBindingContext& binder) override;

    float getPointScale() const override;

    Value createViewNodeWrapper(const Ref<ViewNode>& viewNode, bool wrapInPlatformReference) override;

    bool supportsClassNameNatively(const StringBox& className) override;

    Ref<IViewTransaction> createViewTransaction(const Ref<MainThreadManager>& mainThreadManager,
                                                bool shouldDefer) override;

    Ref<IBitmapFactory> getViewRasterBitmapFactory() const override;

private:
    PlatformType _platformType;
    ILogger& _logger;
    Ref<ITerminalViewClass> _baseClass;
    mutable Mutex _mutex;
    FlatMap<StringBox, Ref<ITerminalViewClass>> _classes;
    FlatSet<StringBox> _reportedUnknownClasses;

    StringBox getClassName(const ITerminalViewClass& viewClass) const;
    Ref<ITerminalViewClass> resolveClass(const StringBox& className);
};

} // namespace Valdi
