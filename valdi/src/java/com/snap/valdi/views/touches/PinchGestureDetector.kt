package com.snap.valdi.views.touches

import android.content.Context
import android.view.MotionEvent
import android.view.ViewConfiguration
import kotlin.math.sqrt

class PinchGestureDetector(context: Context, private val listener: PinchGestureDetectorListener) {
    private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop

    interface PinchGestureDetectorListener {
        fun onScale(scaleFactor: Float)
        fun onScaleBegin()
        fun onScaleEnd()
    }

    private var initialDistance = 0f
    private var cumulativeScaleFactor = 1f
    private var isScaling = false

    fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (event.pointerCount == 2) {
                    initialDistance = calculateDistance(event)
                }
            }
            MotionEvent.ACTION_MOVE -> {
                // Allows for more than 2 pointers to be present.
                // However, only the first two pointers are used for scaling.
                if (event.pointerCount >= 2) {
                    val currentDistance = calculateDistance(event)
                    if (!isScaling && Math.abs(currentDistance - initialDistance) > touchSlop) {
                        isScaling = true
                        initialDistance = currentDistance
                        listener.onScaleBegin()
                    }
                    if (isScaling) {
                        val scaleFactor = currentDistance / initialDistance
                        cumulativeScaleFactor *= scaleFactor
                        initialDistance = currentDistance
                        listener.onScale(cumulativeScaleFactor)
                    }
                }
            }
            MotionEvent.ACTION_UP -> {
                if (isScaling) {
                    isScaling = false
                    cumulativeScaleFactor = 1f
                    initialDistance = 0f
                    listener.onScaleEnd()
                }
            }
        }
        return true
    }

    /**
     * Resets the detector to its initial state without firing any listener callbacks.
     * Call this from onReset() to handle cases where the gesture is cancelled externally
     * (e.g. ACTION_CANCEL from a parent view intercepting touch), mirroring the V1
     * PinchGestureRecognizer pattern of resetting inner detector state on reset.
     */
    fun reset() {
        isScaling = false
        cumulativeScaleFactor = 1f
        initialDistance = 0f
    }

    private fun calculateDistance(event: MotionEvent, index1: Int = 0, index2: Int = 1): Float {
        val deltaX = event.getX(index2) - event.getX(index1)
        val deltaY = event.getY(index2) - event.getY(index1)
        return sqrt(deltaX * deltaX + deltaY * deltaY)
    }
}
