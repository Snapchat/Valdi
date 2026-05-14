//
//  StandaloneViewTransaction.hpp
//  valdi
//
//  Created by Simon Corsin on 7/26/24.
//

#include "valdi/standalone_runtime/StandaloneViewTransaction.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/standalone_runtime/StandaloneView.hpp"

#include "utils/debugging/Assert.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"

namespace Valdi {

StandaloneViewTransaction::StandaloneViewTransaction() = default;
StandaloneViewTransaction::~StandaloneViewTransaction() = default;

void StandaloneViewTransaction::flush(bool sync) {}

void StandaloneViewTransaction::willUpdateRootView(const Ref<View>& view) {}

void StandaloneViewTransaction::didUpdateRootView(const Ref<View>& view, bool layoutDidBecomeDirty) {
    if (layoutDidBecomeDirty) {
        StandaloneView::unwrap(view)->incrementLayoutDidBecomeDirty();
    }

    auto callbacks = std::move(_pendingOnNextDrawCallbacks);
    for (auto& callback : callbacks) {
        callback();
    }
}

void StandaloneViewTransaction::moveViewToTree(const Ref<View>& view, ViewNodeTree* viewNodeTree, ViewNode* viewNode) {
    if (viewNode == nullptr) {
        return;
    }
    auto debugId = viewNode->getDebugId();
    if (debugId.isEmpty()) {
        return;
    }

    static auto kId = STRING_LITERAL("id");

    if (!debugId.hasPrefix("!")) {
        StandaloneView::unwrap(view)->setAttribute(kId, Value(debugId));
    } else {
        StandaloneView::unwrap(view)->setAttribute(kId, Value::undefined());
    }
}

void StandaloneViewTransaction::insertChildView(const Ref<View>& view,
                                                const Ref<View>& childView,
                                                int index,
                                                const Ref<Animator>& animator) {
    StandaloneView::unwrap(view)->addChild(StandaloneView::unwrap(childView), index);
}

void StandaloneViewTransaction::removeViewFromParent(const Ref<View>& view,
                                                     const Ref<Animator>& animator,
                                                     bool shouldClearViewNode) {
    StandaloneView::unwrap(view)->removeFromParent();
}

void StandaloneViewTransaction::invalidateViewLayout(const Ref<View>& view) {
    StandaloneView::unwrap(view)->invalidateLayout();
}

void StandaloneViewTransaction::setViewFrame(const Ref<View>& view,
                                             const Frame& newFrame,
                                             bool isRightToLeft,
                                             const Ref<Animator>& animator) {
    auto typedView = StandaloneView::unwrap(view);
    typedView->setFrame(newFrame);
    typedView->setIsRightToLeft(isRightToLeft);
}

void StandaloneViewTransaction::setViewScrollSpecs(const Ref<View>& view,
                                                   const Point& directionDependentContentOffset,
                                                   const Size& contentSize,
                                                   bool animated) {}

void StandaloneViewTransaction::setViewLoadedAsset(const Ref<View>& view,
                                                   const Ref<LoadedAsset>& loadedAsset,
                                                   bool shouldDrawFlipped) {}

void StandaloneViewTransaction::layoutView(const Ref<View>& view) {}

void StandaloneViewTransaction::cancelAllViewAnimations(const Ref<View>& view) {}

void StandaloneViewTransaction::willEnqueueViewToPool(const Ref<View>& view, Function<void(View&)> onEnqueue) {
    if (StandaloneView::unwrap(view)->allowsPooling()) {
        onEnqueue(*view);
    }
}

void StandaloneViewTransaction::snapshotView(const Ref<View>& view, Function<void(Result<BytesView>)> cb) {
    cb(BytesView());
}

void StandaloneViewTransaction::flushAnimator(const Ref<Animator>& animator, const Value& completionCallback) {}

void StandaloneViewTransaction::cancelAnimator(const Ref<Animator>& animator) {}

void StandaloneViewTransaction::scheduleOnNextDraw(const Ref<View>& rootView, DispatchFunction callback) {
    _pendingOnNextDrawCallbacks.emplace_back(std::move(callback));
}

void StandaloneViewTransaction::executeInTransactionThread(DispatchFunction executeFn) {
    executeFn();
}

} // namespace Valdi
