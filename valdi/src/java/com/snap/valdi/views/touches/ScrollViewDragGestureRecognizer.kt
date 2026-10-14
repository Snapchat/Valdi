package com.snap.valdi.views.touches

import android.view.MotionEvent
import android.view.View
import android.widget.TextView
import com.snap.valdi.views.ValdiTouchTarget

class ScrollViewDragGestureRecognizer(
    view: View,
    listener: DragGestureRecognizerListener,
    private val onStopScrolling: (() -> Unit)? = null
): DragGestureRecognizer(view, listener) {

    var isHorizontalScroll = false
    var isAnimatingScroll = false
    var stopScrollingOnTouch = false

    var bounces = true
    var bouncesFromDragAtStart = true
    var bouncesFromDragAtEnd = true
    var bouncesHorizontalWithSmallContent = false
    var bouncesVerticalWithSmallContent = false

    var cancelsTouchesOnScroll = true

    override fun shouldRecognize(distanceX: Float, distanceY: Float): Boolean {
        // If we will be overscrolling, the drag gesture can only succeed
        // only if the bouncing is allowed in that situation
        if (!bounces || !bouncesFromDragAtStart || !bouncesFromDragAtEnd) {
            val afterDragDirection = this.computeDirection(offsetX, offsetY)
            val afterDragOverscrolling = this.computeOverscrolling(afterDragDirection)
            // Trying to bounce
            if (!bounces && afterDragOverscrolling) {
                return false
            }
            // Trying to bounce when at start
            if (!bouncesFromDragAtStart && afterDragDirection < 0 && afterDragOverscrolling) {
                return false
            }
            // Trying to bounce when at end
            if (!bouncesFromDragAtEnd && afterDragDirection > 0 && afterDragOverscrolling) {
                return false
            }
        }
        // Trying to bounce horizontally with small content
        if (isHorizontalScroll
            && !bouncesHorizontalWithSmallContent
            && !view.canScrollHorizontally(1)
            && !view.canScrollHorizontally(-1)) {
            return false
        }
        // Trying to bounce vertically with small content
        if (!isHorizontalScroll
            && !bouncesVerticalWithSmallContent
            && !view.canScrollVertically(1)
            && !view.canScrollVertically(-1)) {
            return false
        }
        // Only succeed if the drag is in the proper direction
        return if (isHorizontalScroll) {
            Math.abs(distanceX) > Math.abs(distanceY)
        } else {
            Math.abs(distanceY) > Math.abs(distanceX)
        }
    }

    override fun shouldCancelOtherGesturesOnStart(): Boolean {
        return cancelsTouchesOnScroll
    }

    override fun onUpdate(event: MotionEvent) {
        if (stopScrollingOnTouch && isAnimatingScroll && event.actionMasked == MotionEvent.ACTION_DOWN && state == ValdiGestureRecognizerState.POSSIBLE) {
            onStopScrolling?.invoke()
        }
        super.onUpdate(event)

        // Immediately start the scroll gesture on down if we are already animating.
        if (isAnimatingScroll && event.actionMasked == MotionEvent.ACTION_DOWN && state == ValdiGestureRecognizerState.POSSIBLE) {
            recognize(event)
        }
    }

    private fun computeDirection(offsetX: Float, offsetY: Float): Int {
        if (isHorizontalScroll) {
            return if (offsetX < 0) 1 else -1
        } else {
            return if (offsetY < 0) 1 else -1
        }
    }

    private fun computeOverscrolling(direction: Int): Boolean {
        if (isHorizontalScroll) {
            return !view.canScrollHorizontally(direction)
        } else {
            return !view.canScrollVertically(direction)
        }
    }

}
