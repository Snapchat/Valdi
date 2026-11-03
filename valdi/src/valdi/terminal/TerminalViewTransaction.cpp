#include "valdi/terminal/TerminalViewTransaction.hpp"

#include "valdi/terminal/Host/TerminalRoot.hpp"
#include "valdi/terminal/TerminalView.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"

namespace Valdi {

TerminalViewTransaction::TerminalViewTransaction() = default;
TerminalViewTransaction::~TerminalViewTransaction() = default;

void TerminalViewTransaction::flush(bool sync) {
    if (sync && _root != nullptr) {
        _root->paintNow();
    }
}

void TerminalViewTransaction::willUpdateRootView(const Ref<View>& view) {
    auto terminalView = TerminalView::unwrap(view);
    _root = terminalView != nullptr ? terminalView->getRoot() : nullptr;
}

void TerminalViewTransaction::didUpdateRootView(const Ref<View>& view, bool /*layoutDidBecomeDirty*/) {
    auto callbacks = std::move(_pendingOnNextDrawCallbacks);
    _pendingOnNextDrawCallbacks.clear();
    for (auto& callback : callbacks) {
        callback();
    }

    auto terminalView = TerminalView::unwrap(view);
    auto root = terminalView != nullptr ? terminalView->getRoot() : nullptr;
    if (root != nullptr) {
        root->scheduleFrame();
    }
    _root = root;
}

void TerminalViewTransaction::moveViewToTree(const Ref<View>& view,
                                             ViewNodeTree* /*viewNodeTree*/,
                                             ViewNode* viewNode) {
    auto terminalView = TerminalView::unwrap(view);
    if (terminalView != nullptr) {
        terminalView->setViewNode(viewNode);
    }
}

void TerminalViewTransaction::insertChildView(const Ref<View>& view,
                                              const Ref<View>& childView,
                                              int index,
                                              const Ref<Animator>& /*animator*/) {
    auto parent = TerminalView::unwrap(view);
    auto child = TerminalView::unwrap(childView);
    if (parent != nullptr && child != nullptr) {
        parent->insertChild(child, static_cast<size_t>(std::max(0, index)));
    }
}

void TerminalViewTransaction::removeViewFromParent(const Ref<View>& view,
                                                   const Ref<Animator>& /*animator*/,
                                                   bool /*shouldClearViewNode*/) {
    auto terminalView = TerminalView::unwrap(view);
    if (terminalView != nullptr) {
        terminalView->removeFromParent();
    }
}

void TerminalViewTransaction::invalidateViewLayout(const Ref<View>& /*view*/) {}

void TerminalViewTransaction::setViewFrame(const Ref<View>& view,
                                           const Frame& newFrame,
                                           bool /*isRightToLeft*/,
                                           const Ref<Animator>& /*animator*/) {
    auto terminalView = TerminalView::unwrap(view);
    if (terminalView != nullptr) {
        terminalView->setFrame(newFrame);
    }
}

void TerminalViewTransaction::setViewScrollSpecs(const Ref<View>& view,
                                                 const Point& directionDependentContentOffset,
                                                 const Size& contentSize,
                                                 bool /*animated*/) {
    auto terminalView = TerminalView::unwrap(view);
    if (terminalView != nullptr) {
        terminalView->setScrollSpecs(directionDependentContentOffset, contentSize);
    }
}

void TerminalViewTransaction::setViewLoadedAsset(const Ref<View>& /*view*/,
                                                 const Ref<LoadedAsset>& /*loadedAsset*/,
                                                 bool /*shouldDrawFlipped*/) {}

void TerminalViewTransaction::layoutView(const Ref<View>& /*view*/) {}

void TerminalViewTransaction::cancelAllViewAnimations(const Ref<View>& /*view*/) {}

// Views carry attribute state that is only reset when the node detaches, so they are never pooled.
void TerminalViewTransaction::willEnqueueViewToPool(const Ref<View>& /*view*/, Function<void(View&)> /*onEnqueue*/) {}

void TerminalViewTransaction::snapshotView(const Ref<View>& /*view*/, Function<void(Result<BytesView>)> cb) {
    cb(BytesView());
}

void TerminalViewTransaction::flushAnimator(const Ref<Animator>& /*animator*/, const Value& /*completionCallback*/) {}

void TerminalViewTransaction::cancelAnimator(const Ref<Animator>& /*animator*/) {}

void TerminalViewTransaction::scheduleOnNextDraw(const Ref<View>& /*rootView*/, DispatchFunction callback) {
    _pendingOnNextDrawCallbacks.emplace_back(std::move(callback));
}

void TerminalViewTransaction::executeInTransactionThread(DispatchFunction executeFn) {
    executeFn();
}

} // namespace Valdi
