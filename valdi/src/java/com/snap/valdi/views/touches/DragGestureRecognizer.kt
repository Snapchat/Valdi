package com.snap.valdi.views.touches

import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.View

interface DragGestureRecognizerListener {
    fun onRecognized(gesture: DragGestureRecognizer, state: ValdiGestureRecognizerState, x: Int, y: Int, offsetX: Int, offsetY: Int, velocityX: Float, velocityY: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>)
    fun shouldBegin(gesture: DragGestureRecognizer, x: Int, y: Int, offsetX: Int, offsetY: Int, velocityX: Float, velocityY: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>): Boolean
}

open class DragGestureRecognizer(
        view: View,
        val listener: DragGestureRecognizerListener
) : AndroidDetectorGestureRecognizer(view, false) {

    protected var offsetX = 0f
    protected var offsetY = 0f
    private var velocityX = 0f
    private var velocityY = 0f

    // Offset values adjusted for scaling
    private var adjustedOffsetX = 0f
    private var adjustedOffsetY = 0f

    private var velocityTracker: VelocityTracker? = null

    // Todo: onScroll is called from cpp so we cannot pass in the adjusted MotionEvent from
    //  TouchDispatcher. Consider refactor this to android.
    override fun onScroll(start: MotionEvent?, end: MotionEvent, distanceX: Float, distanceY: Float): Boolean {
        if (start == null) return false

        offsetX -= distanceX
        offsetY -= distanceY

        adjustedOffsetX = (offsetX / cumulativeScaleX)
        adjustedOffsetY = (offsetY / cumulativeScaleY)

        if (state == ValdiGestureRecognizerState.POSSIBLE) {
            if (shouldRecognize(distanceX, distanceY)) {
                recognize(start)
            }
        }

        return true
    }

    open fun shouldRecognize(offsetX: Float, offsetY: Float): Boolean {
        return true
    }

    fun recognize(event: MotionEvent) {
        if (state == ValdiGestureRecognizerState.POSSIBLE) {
            updateState(ValdiGestureRecognizerState.BEGAN)
        }
    }

    protected fun cancelWithoutMomentum(event: MotionEvent) {
        velocityX = 0f
        velocityY = 0f
        cancel(event)
    }

    private fun recycleVelocityTracker() {
        this.velocityTracker?.recycle()
        this.velocityTracker = null
    }

    override fun onUpdate(event: MotionEvent) {
        // Force the event to use raw screen coordinates in order to properly support dragging items that are
        // being translated as they're being dragged
        val convertedEvent = MotionEvent.obtain(event)
        convertedEvent.offsetLocation(event.rawX - event.x, event.rawY - event.y)
        gestureDetector.onTouchEvent(convertedEvent)

        if (isActive) {
            when (convertedEvent.actionMasked) {
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    updateState(ValdiGestureRecognizerState.ENDED)
                }
                else -> {}
            }

            if (this.velocityTracker == null) {
                this.velocityTracker = VelocityTracker.obtain()
            }
            val velocityTracker = this.velocityTracker!!
            velocityTracker.addMovement(convertedEvent)
            velocityTracker.computeCurrentVelocity(1000)

            velocityX = velocityTracker.xVelocity
            velocityY = velocityTracker.yVelocity

            if (state == ValdiGestureRecognizerState.ENDED) {
                recycleVelocityTracker()
            }
        }

        convertedEvent.recycle()
    }

    override fun onProcess() {
        listener.onRecognized(this, state, adjustedX, adjustedY, adjustedOffsetX.toInt(), adjustedOffsetY.toInt(), velocityX, velocityY, pointerCount, pointerLocations)
    }

    override fun shouldBegin(): Boolean {
        return listener.shouldBegin(this, adjustedX, adjustedY, adjustedOffsetX.toInt(), adjustedOffsetY.toInt(), velocityX, velocityY, pointerCount, pointerLocations)
    }

    override fun onReset(event: MotionEvent) {
        super.onReset(event)

        offsetX = 0f
        offsetY = 0f
        velocityX = 0f
        velocityY = 0f
        adjustedOffsetX = 0f
        adjustedOffsetY = 0f
        recycleVelocityTracker()
    }

    override fun onDestroy() {
        super.onDestroy()
        recycleVelocityTracker()
    }
}
