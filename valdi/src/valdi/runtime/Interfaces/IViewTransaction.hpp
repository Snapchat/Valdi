//
//  IViewTransaction.hpp
//  valdi
//
//  Created by Simon Corsin on 7/26/24.
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace Valdi {

class View;
class ViewNodeTree;
class ViewNode;
class Animator;
class LoadedAsset;
struct Frame;
struct Point;
struct Size;
class Value;

using ApplyViewAttributeFn = Function<void(const Ref<View>& view, const Ref<Animator>& animator)>;

/**
 ViewTransaction allows the runtime to interact with a view hierarchy. Implementations
 can either immediately mutate the view hierarchy as the transaction calls are being made,
 or can defer the operations until flush() is called.
 */
class IViewTransaction : public SimpleRefCountable {
public:
    /**
    Submit all the pending operations. This will be a no-op if the ViewTransaction is direct.
    If sync is true, the flush is expected to occur synchronously.
     */
    virtual void flush(bool sync) = 0;

    /**
      Notifies that we are about to process some mutations within the tree represented by the given root view
     */
    virtual void willUpdateRootView(const Ref<View>& view) = 0;

    /**
      Notifies that we finished processing some mutations within the tree represented by the given root view
     */
    virtual void didUpdateRootView(const Ref<View>& view, bool layoutDidBecomeDirty) = 0;

    /**
    Move the view into the given ViewNodeTree associated with the given node
     */
    virtual void moveViewToTree(const Ref<View>& view, ViewNodeTree* viewNodeTree, ViewNode* viewNode) = 0;

    /**
    Insert a view as a child view into a parent view
     */
    virtual void insertChildView(const Ref<View>& view,
                                 const Ref<View>& childView,
                                 int index,
                                 const Ref<Animator>& animator) = 0;

    /**
    Remove a child view from its parent
     */
    virtual void removeViewFromParent(const Ref<View>& view,
                                      const Ref<Animator>& animator,
                                      bool shouldClearViewNode) = 0;

    /**
     Notifies that the layout has been invalidated and must be recalculated.
     */
    virtual void invalidateViewLayout(const Ref<View>& view) = 0;

    /**
     Notifies that a view has received a new frame.
     If an animator is provided, the frame transition should be animated.
     */
    virtual void setViewFrame(const Ref<View>& view,
                              const Frame& newFrame,
                              bool isRightToLeft,
                              const Ref<Animator>& animator) = 0;

    /**
     Notifies a View that its scrollable specifications has changed.
     This will be called for any backing view that had scroll attributes bounds and
     whenever the the contentOffset or contentWidth have changed.
     */
    virtual void setViewScrollSpecs(const Ref<View>& view,
                                    const Point& directionDependentContentOffset,
                                    const Size& contentSize,
                                    bool animated) = 0;

    /**
     Notifies a View that it has received a new loaded asset
     */
    virtual void setViewLoadedAsset(const Ref<View>& view,
                                    const Ref<LoadedAsset>& loadedAsset,
                                    bool shouldDrawFlipped) = 0;

    /**
      Perform an immediate layout on the given view
     */
    virtual void layoutView(const Ref<View>& view) = 0;

    /**
      Cancel all the animations that are running within the given view
     */
    virtual void cancelAllViewAnimations(const Ref<View>& view) = 0;

    /**
     Called whenever a View will be enqueued to the pool.
     If this returns false, the view will not be enqueued.
     The shouldDefer boolean pointer can be set to true
     to notify the framework to wait until the next runloop
     before enqueueing the view.
     */
    virtual void willEnqueueViewToPool(const Ref<View>& view, Function<void(View&)> onEnqueue) = 0;

    /**
     Take a snapshot of the view
     */
    virtual void snapshotView(const Ref<View>& view, Function<void(Result<BytesView>)> cb) = 0;

    /**
    Flush the given animator. This will trigger the animations that were registered inside it.
     */
    virtual void flushAnimator(const Ref<Animator>& animator, const Value& completionCallback) = 0;

    /**
    Cancel the animator. This will cancel all animations that were registered inside it.
     */
    virtual void cancelAnimator(const Ref<Animator>& animator) = 0;

    /**
     Schedule a callback to run after the next draw/commit pass for the given root view completes.
     This is a platform rendering-pipeline hook, not a strict guarantee that the frame has already
     been physically presented on screen.
     */
    virtual void scheduleOnNextDraw(const Ref<View>& rootView, DispatchFunction callback) = 0;

    /**
      Execute an arbitrary callback in the thread where the transaction will be flushed.
     */
    virtual void executeInTransactionThread(DispatchFunction executeFn) = 0;
};

} // namespace Valdi
