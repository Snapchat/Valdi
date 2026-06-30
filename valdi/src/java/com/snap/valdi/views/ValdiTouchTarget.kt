package com.snap.valdi.views

import android.view.MotionEvent

// Possible choice given to the underlying view to decide what to do after receiving an event
enum class ValdiTouchEventResult {
    ConsumeEventAndCancelOtherGestures,
    IgnoreEvent,
}

// Marking a View with this interface will allow it to intercept touch events in
// the Valdi touch dispatching framework
interface ValdiTouchTarget {

    // Override this method to decide what to do with touch events
    // This method should call "this.dispatchTouchEvent()" on the underlying view
    // (If it wants the underlying view to receive the android event at all)
    fun processTouchEvent(event: MotionEvent): ValdiTouchEventResult

    // Override this when Valdi gesture recognizers attached to the same view
    // should receive the touch sequence alongside the touch target.
    fun allowsSameViewGestureRecognizers(): Boolean = false

    // Override this method to perform a customized hit test of an event.
    // This can be used to pass an event to an underneath view.
    fun hitTest(event: MotionEvent): Boolean? = null
}
