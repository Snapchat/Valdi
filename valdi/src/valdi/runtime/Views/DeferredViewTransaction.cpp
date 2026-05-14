//
//  DeferredViewTransaction.cpp
//  valdi
//
//  Created by Simon Corsin on 7/26/24.
//

#include "valdi/runtime/Views/DeferredViewTransaction.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"
#include "valdi_core/cpp/Utils/TrackedLock.hpp"

namespace Valdi {

DeferredViewTransaction::DeferredViewTransaction(IViewManager& viewManager, MainThreadManager& mainThreadManager)
    : _viewManager(viewManager), _mainThreadManager(mainThreadManager), _context(Context::current()) {}

DeferredViewTransaction::~DeferredViewTransaction() = default;

void DeferredViewTransaction::flush(bool sync) {
    if (_operations.empty()) {
        return;
    }

    DispatchFunction dispatchFn = [viewManager = &_viewManager, operations = std::move(_operations)]() {
        auto transaction = viewManager->createViewTransaction(nullptr, false);

        for (const auto& operation : operations) {
            operation(*transaction);
        }

        transaction->flush(/* sync */ false);
    };

    if (sync) {
        DropAllTrackedLocks dropAllTrackedLocks;
        _mainThreadManager.dispatchSync(_context, std::move(dispatchFn));
    } else {
        _mainThreadManager.dispatch(_context, std::move(dispatchFn));
    }
}

void DeferredViewTransaction::enqueue(DeferredViewTransactionOperation&& operation) {
    _operations.emplace_back(std::move(operation));
}

void DeferredViewTransaction::willUpdateRootView(const Ref<View>& view) {
    enqueue([=](IViewTransaction& transaction) { transaction.willUpdateRootView(view); });
}

void DeferredViewTransaction::didUpdateRootView(const Ref<View>& view, bool layoutDidBecomeDirty) {
    enqueue([=](IViewTransaction& transaction) { transaction.didUpdateRootView(view, layoutDidBecomeDirty); });
}

void DeferredViewTransaction::moveViewToTree(const Ref<View>& view, ViewNodeTree* viewNodeTree, ViewNode* viewNode) {
    enqueue(
        [view, viewNodeTree = strongSmallRef(viewNodeTree), viewNode = strongSmallRef(viewNode)](
            IViewTransaction& transaction) { transaction.moveViewToTree(view, viewNodeTree.get(), viewNode.get()); });
}

void DeferredViewTransaction::insertChildView(const Ref<View>& view,
                                              const Ref<View>& childView,
                                              int index,
                                              const Ref<Animator>& animator) {
    enqueue([=](IViewTransaction& transaction) { transaction.insertChildView(view, childView, index, animator); });
}

void DeferredViewTransaction::removeViewFromParent(const Ref<View>& view,
                                                   const Ref<Animator>& animator,
                                                   bool shouldClearViewNode) {
    enqueue(
        [=](IViewTransaction& transaction) { transaction.removeViewFromParent(view, animator, shouldClearViewNode); });
}

void DeferredViewTransaction::invalidateViewLayout(const Ref<View>& view) {
    enqueue([=](IViewTransaction& transaction) { transaction.invalidateViewLayout(view); });
}

void DeferredViewTransaction::setViewFrame(const Ref<View>& view,
                                           const Frame& newFrame,
                                           bool isRightToLeft,
                                           const Ref<Animator>& animator) {
    enqueue([=](IViewTransaction& transaction) { transaction.setViewFrame(view, newFrame, isRightToLeft, animator); });
}

void DeferredViewTransaction::setViewScrollSpecs(const Ref<View>& view,
                                                 const Point& directionDependentContentOffset,
                                                 const Size& contentSize,
                                                 bool animated) {
    enqueue([=](IViewTransaction& transaction) {
        transaction.setViewScrollSpecs(view, directionDependentContentOffset, contentSize, animated);
    });
}

void DeferredViewTransaction::setViewLoadedAsset(const Ref<View>& view,
                                                 const Ref<LoadedAsset>& loadedAsset,
                                                 bool shouldDrawFlipped) {
    enqueue(
        [=](IViewTransaction& transaction) { transaction.setViewLoadedAsset(view, loadedAsset, shouldDrawFlipped); });
}

void DeferredViewTransaction::layoutView(const Ref<View>& view) {
    enqueue([=](IViewTransaction& transaction) { transaction.layoutView(view); });
}

void DeferredViewTransaction::cancelAllViewAnimations(const Ref<View>& view) {
    enqueue([=](IViewTransaction& transaction) { transaction.cancelAllViewAnimations(view); });
}

void DeferredViewTransaction::willEnqueueViewToPool(const Ref<View>& view, Function<void(View&)> onEnqueue) {
    enqueue([view, onEnqueue = std::move(onEnqueue)](IViewTransaction& transaction) {
        transaction.willEnqueueViewToPool(view, onEnqueue);
    });
}

void DeferredViewTransaction::snapshotView(const Ref<View>& view, Function<void(Result<BytesView>)> cb) {
    enqueue([view, cb = std::move(cb)](IViewTransaction& transaction) { transaction.snapshotView(view, cb); });
}

void DeferredViewTransaction::flushAnimator(const Ref<Animator>& animator, const Value& completionCallback) {
    enqueue([=](IViewTransaction& transaction) { transaction.flushAnimator(animator, completionCallback); });
}

void DeferredViewTransaction::cancelAnimator(const Ref<Animator>& animator) {
    enqueue([=](IViewTransaction& transaction) { transaction.cancelAnimator(animator); });
}

void DeferredViewTransaction::scheduleOnNextDraw(const Ref<View>& rootView, DispatchFunction callback) {
    enqueue([rootView, callback = std::move(callback)](IViewTransaction& transaction) mutable {
        transaction.scheduleOnNextDraw(rootView, std::move(callback));
    });
}

void DeferredViewTransaction::executeInTransactionThread(DispatchFunction executeFn) {
    enqueue([executeFn = std::move(executeFn)](IViewTransaction& transaction) {
        transaction.executeInTransactionThread(executeFn);
    });
}

} // namespace Valdi
