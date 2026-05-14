//
//  AndroidViewTransaction.cpp
//  valdi-android
//
//  Created by Simon Corsin on 7/29/24.
//

#include "valdi/android/AndroidViewTransaction.hpp"
#include "valdi/android/AndroidViewHolder.hpp"
#include "valdi/android/DeferredViewOperations.hpp"

#include "valdi/android/NativeBridge.hpp"
#include "valdi/android/ViewManager.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi_core/NativeAnimator.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

#include "valdi/android/CppPromiseCallbackJNI.hpp"
#include "valdi/android/JavaValueDelegate.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"

namespace ValdiAndroid {

AndroidViewTransaction::AndroidViewTransaction(ViewManager& viewManager) : _viewManager(viewManager) {}

AndroidViewTransaction::~AndroidViewTransaction() {
    for (const auto& pendingCallback : _pendingOnNextDrawCallbacks) {
        delete reinterpret_cast<Valdi::DispatchFunction*>(pendingCallback.callbackHandle);
    }
}

void AndroidViewTransaction::flush(bool sync) {
    _viewManager.flushViewOperations(std::move(_viewOperations), sync);
}

void AndroidViewTransaction::willUpdateRootView(const Valdi::Ref<Valdi::View>& view) {
    getViewOperations().enqueueBeginRenderingView(view);
}

void AndroidViewTransaction::didUpdateRootView(const Valdi::Ref<Valdi::View>& view, bool layoutDidBecomeDirty) {
    getViewOperations().enqueueEndRenderingView(view, layoutDidBecomeDirty);

    for (const auto& pendingCallback : _pendingOnNextDrawCallbacks) {
        auto callbackRootView = pendingCallback.rootView != nullptr ? pendingCallback.rootView : view;
        if (callbackRootView == nullptr) {
            delete reinterpret_cast<Valdi::DispatchFunction*>(pendingCallback.callbackHandle);
            continue;
        }

        getViewOperations().enqueueOnNextDraw(callbackRootView, pendingCallback.callbackHandle);
    }

    _pendingOnNextDrawCallbacks.clear();
}

void AndroidViewTransaction::moveViewToTree(const Valdi::Ref<Valdi::View>& view,
                                            Valdi::ViewNodeTree* viewNodeTree,
                                            Valdi::ViewNode* viewNode) {
    auto userData = viewNodeTree->getContext()->getUserData();
    if (userData != nullptr) {
        auto viewNodeId = viewNode != nullptr ? viewNode->getRawId() : 0;
        getViewOperations().enqueueMovedToTree(view, Valdi::Value(userData), static_cast<int32_t>(viewNodeId));
    }
}

void AndroidViewTransaction::insertChildView(const Valdi::Ref<Valdi::View>& view,
                                             const Valdi::Ref<Valdi::View>& childView,
                                             int index,
                                             const Valdi::Ref<Valdi::Animator>& animator) {
    getViewOperations().enqueueMoveToParent(childView, view, static_cast<int32_t>(index));
}

void AndroidViewTransaction::removeViewFromParent(const Valdi::Ref<Valdi::View>& view,
                                                  const Valdi::Ref<Valdi::Animator>& animator,
                                                  bool shouldClearViewNode) {
    getViewOperations().enqueueRemoveFromParent(view, shouldClearViewNode);
}

void AndroidViewTransaction::invalidateViewLayout(const Valdi::Ref<Valdi::View>& view) {
    JavaCache::get().getViewRefInvalidateLayoutMethod().call(fromValdiView(view));
}

void AndroidViewTransaction::setViewFrame(const Valdi::Ref<Valdi::View>& view,
                                          const Valdi::Frame& newFrame,
                                          bool isRightToLeft,
                                          const Valdi::Ref<Valdi::Animator>& animator) {
    auto androidView = Valdi::castOrNull<AndroidViewHolder>(view);
    auto intX = androidView->convertPointsToPixels(newFrame.x);
    auto intY = androidView->convertPointsToPixels(newFrame.y);
    auto intWidth = androidView->convertPointsToPixels(newFrame.width);
    auto intHeight = androidView->convertPointsToPixels(newFrame.height);

    getViewOperations().enqueueSetFrame(view, intX, intY, intWidth, intHeight, isRightToLeft, animator);
}

void AndroidViewTransaction::setViewScrollSpecs(const Valdi::Ref<Valdi::View>& view,
                                                const Valdi::Point& directionDependentContentOffset,
                                                const Valdi::Size& contentSize,
                                                bool animated) {
    auto androidView = Valdi::castOrNull<AndroidViewHolder>(view);
    auto intContentOffsetX = androidView->convertPointsToPixels(directionDependentContentOffset.x);
    auto intContentOffsetY = androidView->convertPointsToPixels(directionDependentContentOffset.y);
    auto intContentWidth = androidView->convertPointsToPixels(contentSize.width);
    auto intContentHeight = androidView->convertPointsToPixels(contentSize.height);

    getViewOperations().enqueueSetScrollableSpecs(
        view, intContentOffsetX, intContentOffsetY, intContentWidth, intContentHeight, animated);
}

void AndroidViewTransaction::setViewLoadedAsset(const Valdi::Ref<Valdi::View>& view,
                                                const Valdi::Ref<Valdi::LoadedAsset>& loadedAsset,
                                                bool shouldDrawFlipped) {
    getViewOperations().enqueueSetLoadedAsset(view, loadedAsset, shouldDrawFlipped);
}

void AndroidViewTransaction::layoutView(const Valdi::Ref<Valdi::View>& view) {
    JavaCache::get().getViewRefLayoutMethod().call(fromValdiView(view));
}

void AndroidViewTransaction::cancelAllViewAnimations(const Valdi::Ref<Valdi::View>& view) {
    JavaCache::get().getViewRefCancelAllAnimationsMethod().call(fromValdiView(view));
}

void AndroidViewTransaction::willEnqueueViewToPool(const Valdi::Ref<Valdi::View>& view,
                                                   Valdi::Function<void(Valdi::View&)> onEnqueue) {
    if (!Valdi::castOrNull<AndroidViewHolder>(view)->isRecyclable()) {
        return;
    }

    getViewOperations().enqueueAddedToPool(view);

    onEnqueue(*view);
}

void AndroidViewTransaction::snapshotView(const Valdi::Ref<Valdi::View>& view,
                                          Valdi::Function<void(Valdi::Result<Valdi::BytesView>)> cb) {
    auto callback = Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
        [cb = std::move(cb)](const Valdi::ValueFunctionCallContext& callContext) -> Valdi::Value {
            auto value = callContext.getParameter(0);
            auto typedArray = value.getTypedArray();
            cb(typedArray != nullptr ? typedArray->getBuffer() : Valdi::BytesView());
            return Valdi::Value::undefined();
        });

    auto callbackAsActionObject = ValdiAndroid::toJavaObject(JavaEnv(), callback);
    ValdiFunctionType valdiFunction(JavaEnv(), callbackAsActionObject.stealLocalRef());
    JavaCache::get().getViewRefSnapshotMethod().call(fromValdiView(view), valdiFunction);
}

void AndroidViewTransaction::flushAnimator(const Valdi::Ref<Valdi::Animator>& animator,
                                           const Valdi::Value& completionCallback) {
    flush(/* sync */ false);
    animator->getNativeAnimator()->flushAnimations(completionCallback);
}

void AndroidViewTransaction::cancelAnimator(const Valdi::Ref<Valdi::Animator>& animator) {
    animator->getNativeAnimator()->cancel();
}

void AndroidViewTransaction::scheduleOnNextDraw(const Valdi::Ref<Valdi::View>& rootView,
                                                Valdi::DispatchFunction callback) {
    PendingOnNextDrawCallback pendingCallback;
    pendingCallback.rootView = rootView;
    pendingCallback.callbackHandle = reinterpret_cast<int64_t>(new Valdi::DispatchFunction(std::move(callback)));
    _pendingOnNextDrawCallbacks.emplace_back(std::move(pendingCallback));
}

void AndroidViewTransaction::executeInTransactionThread(Valdi::DispatchFunction executeFn) {
    executeFn();
}

DeferredViewOperations& AndroidViewTransaction::getViewOperations() {
    if (_viewOperations == nullptr) {
        _viewOperations = _viewManager.makeViewOperations();
    }

    return *_viewOperations;
}

} // namespace ValdiAndroid
