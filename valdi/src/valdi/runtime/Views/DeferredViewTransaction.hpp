//
//  DeferredViewTransaction.hpp
//  valdi
//
//  Created by Simon Corsin on 7/26/24.
//

#pragma once

#include "valdi/runtime/Interfaces/IViewTransaction.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include "utils/base/NonCopyable.hpp"
#include <vector>

namespace Valdi {

class IViewManager;

using DeferredViewTransactionOperation = Valdi::Function<void(IViewTransaction&)>;
class IViewManager;
class MainThreadManager;
class Context;

/**
 Implementation of IViewTransaction which will defer all operations into a nested view transaction
 provided by the given IViewManager, and executed in the main thread.
 */
class DeferredViewTransaction : public IViewTransaction {
public:
    DeferredViewTransaction(IViewManager& viewManager, MainThreadManager& mainThreadManager);
    ~DeferredViewTransaction() override;

    void flush(bool sync) override;

    void willUpdateRootView(const Ref<View>& view) override;

    void didUpdateRootView(const Ref<View>& view, bool layoutDidBecomeDirty) override;

    void moveViewToTree(const Ref<View>& view, ViewNodeTree* viewNodeTree, ViewNode* viewNode) override;

    void insertChildView(const Ref<View>& view,
                         const Ref<View>& childView,
                         int index,
                         const Ref<Animator>& animator) override;

    void removeViewFromParent(const Ref<View>& view, const Ref<Animator>& animator, bool shouldClearViewNode) override;

    void invalidateViewLayout(const Ref<View>& view) override;

    void setViewFrame(const Ref<View>& view,
                      const Frame& newFrame,
                      bool isRightToLeft,
                      const Ref<Animator>& animator) override;

    void setViewScrollSpecs(const Ref<View>& view,
                            const Point& directionDependentContentOffset,
                            const Size& contentSize,
                            bool animated) override;

    void setViewLoadedAsset(const Ref<View>& view,
                            const Ref<LoadedAsset>& loadedAsset,
                            bool shouldDrawFlipped) override;

    void layoutView(const Ref<View>& view) override;

    void cancelAllViewAnimations(const Ref<View>& view) override;

    void willEnqueueViewToPool(const Ref<View>& view, Function<void(View&)> onEnqueue) override;

    void snapshotView(const Ref<View>& view, Function<void(Result<BytesView>)> cb) override;

    void flushAnimator(const Ref<Animator>& animator, const Value& completionCallback) override;

    void cancelAnimator(const Ref<Animator>& animator) override;

    void scheduleOnNextDraw(const Ref<View>& rootView, DispatchFunction callback) override;

    void executeInTransactionThread(DispatchFunction executeFn) override;

private:
    IViewManager& _viewManager;
    MainThreadManager& _mainThreadManager;
    Ref<Context> _context;
    std::vector<DeferredViewTransactionOperation> _operations;

    void enqueue(DeferredViewTransactionOperation&& operation);
};

} // namespace Valdi
