package com.snap.valdi.views.touches

import android.view.MotionEvent
import android.view.View
import com.snap.valdi.attributes.impl.gestures.PointerUtils
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.views.ValdiRootView
import kotlin.math.roundToInt

enum class ValdiGestureRecognizerState {
    // Default state, the gesture is not recognized yet.
    POSSIBLE,
    // The gesture failed to be recognized. It will be reset to possible state
    FAILED,
    // The gesture was recognized and just started.
    BEGAN,
    // The gesture was previously recognized and is responding for a continuous gesture
    CHANGED,
    // The gesture was completed.
    ENDED
}

abstract class ValdiGestureRecognizer(val view: View) {

    var state: ValdiGestureRecognizerState = ValdiGestureRecognizerState.POSSIBLE
        private set

    var x = 0
        private set
    var y = 0
        private set
    var pointerCount = 0
        private set
    // The cumulative scaleX of `view`
    var cumulativeScaleX = 1.0f
        private set
    // The cumulative scaleY of `view`
    var cumulativeScaleY = 1.0f
        private set
    // The x value adjusted for scaling/translations
    var adjustedX = 0
        private set
    // The y value adjusted for scaling/translation
    var adjustedY = 0
        private set

    val pointerLocations: MutableList<ValdiGesturePointer> = mutableListOf()
    // Used for calling getLocationOnScreen to avoid allocating a new array every time
    val location = IntArray(2)

    val isActive: Boolean
        get() {
            return when (state) {
                ValdiGestureRecognizerState.BEGAN, ValdiGestureRecognizerState.CHANGED, ValdiGestureRecognizerState.ENDED -> true
                else -> false
            }
        }

    /**
     * Is `true` if this gesture was prevented from entering .BEGAN state. This indicates that a
     * predicate failed the gesture and went from .POSSIBLE -> .FAILED state.
     */
    var hasFailed = false
        private set

    var destroyed = false
        private set

    private var wasProcessed = false
    private var updating = false
    private var deferredNewState: ValdiGestureRecognizerState? = null

    protected val newMultiTouchExperience = TouchDispatcher.newMultiTouchExperience(view)

    open fun requiresFailureOf(other: ValdiGestureRecognizer): Boolean {
        return false
    }

    open fun canRecognizeSimultaneously(other: ValdiGestureRecognizer): Boolean {
        return false
    }

    open fun shouldCancelOtherGesturesOnStart(): Boolean {
        return false
    }

    /**
     * Whether the gesture should prevent a parent View to intercept the touch event.
     */
    open fun shouldPreventInterceptTouchEvent(): Boolean {
        return true
    }

    fun update(event: MotionEvent, isAdjusted:Boolean = false) {
        val wasPossible = state == ValdiGestureRecognizerState.POSSIBLE
        if (state == ValdiGestureRecognizerState.BEGAN) {
            state = ValdiGestureRecognizerState.CHANGED
        }

        // Reset hasFailed at the start of a new gesture sequence (when state is POSSIBLE).
        // This ensures that the hasFailed flag from a previous gesture doesn't persist into
        // the current gesture. Without this, if gesture 1 fails due to a predicate returning
        // false, hasFailed would remain true when gesture 2 begins, causing systems that check
        // hasFailed (like navigation gesture handlers) to incorrectly think gesture 2 also failed.
        if (wasPossible) {
            hasFailed = false
        }

        updating = true

        val requestedNewState = deferredNewState
        if (requestedNewState != null) {
            this.deferredNewState = null
            state = requestedNewState
        } else {
            onUpdate(event)
        }

        updating = false

        // If a gesture reaches began, it's no longer failed even if it failed previously
        if (state == ValdiGestureRecognizerState.BEGAN) {
            hasFailed = false
        }

        if (isActive) {
            if (newMultiTouchExperience) {
                x = event.getX(event.actionIndex).roundToInt()
                y = event.getY(event.actionIndex).roundToInt()
            } else {
                x = event.x.roundToInt()
                y = event.y.roundToInt()
            }

            val (cumulativeScaleX, cumulativeScaleY) = ViewUtils.getCumulativeScale(view)
            this.cumulativeScaleX = cumulativeScaleX
            this.cumulativeScaleY = cumulativeScaleY

            view.getLocationOnScreen(location)
            if (isAdjusted) {
                this.adjustedX = event.x.roundToInt()
                this.adjustedY = event.y.roundToInt()
            } else {
                val (adjustedX, adjustedY) = ViewUtils.adjustedCoordinates(view, event)
                this.adjustedX = adjustedX.roundToInt()
                this.adjustedY = adjustedY.roundToInt()
            }
            pointerCount = event.pointerCount
            pointerLocations.clear()
            PointerUtils.fillPointerLocations(event, pointerLocations)

            if (wasPossible && !shouldBegin()) {
                state = ValdiGestureRecognizerState.FAILED
            }
        }
    }

    fun process() {
        wasProcessed = true
        onProcess()
    }

    val viewIdentifier: String = "${view::class.java.simpleName}-${System.identityHashCode(view)}"
    val gestureIdentifier = "${this::class.java.simpleName}-${System.identityHashCode(this)}"
    fun cancel(event: MotionEvent) {
        pointerCount = event.pointerCount
        if (wasProcessed && state != ValdiGestureRecognizerState.ENDED) {
            // We send an ended event if we were already started (thus acting like a cancel)
            state = ValdiGestureRecognizerState.ENDED
            process()
        }

        // If never processed and is currently failed, keep track of the failed state
        // since state is reset to POSSIBLE
        if (!wasProcessed && state == ValdiGestureRecognizerState.FAILED) {
            hasFailed = true
        } else {
            hasFailed = false;
        }

        wasProcessed = false
        state = ValdiGestureRecognizerState.POSSIBLE
        x = 0
        y = 0
        pointerLocations.clear()
        onReset(event)
    }

    protected fun updateState(newState: ValdiGestureRecognizerState) {
        if (updating) {
            state = newState
        } else {
            // Update state outside of a update() pass.
            // Need to schedule an update
            findTouchDispatcher()?.let {
                if (it.gestureRecognizerWantsDeferredUpdate(this)) {
                    deferredNewState = newState
                }
            }
        }
    }

    private fun findTouchDispatcher(): TouchDispatcher? {
        var current: View? = view
        while (current != null) {
            if (current is ValdiRootView && current.touchDispatcher != null) {
                return current.touchDispatcher
            }

            current = current.parent as? View
        }
        return null
    }

    protected abstract fun shouldBegin(): Boolean

    /**
     * Update the state of the gesture recognizer based on the current event.
     *
     * @param event the current event
     */
    protected abstract fun onUpdate(event: MotionEvent)

    /**
     * Called when a gesture has been accepted and any gesture listeners can be triggered
     */
    protected abstract fun onProcess()

    open fun onReset(event: MotionEvent) {
    }

    /**
     * Called when the gesture detector is removed from a view
     */
    fun destroy() {
        if (!destroyed) {
            destroyed = true
            onDestroy()
        }
    }

    open fun onDestroy() {

    }

    override fun toString(): String {
        return "$gestureIdentifier-$viewIdentifier state-$state"
    }
}
