package com.snap.valdi.views.touches

import android.view.MotionEvent
import kotlin.math.atan2

class RotationGestureDetector(private val listener: OnRotationGestureListener) {

    private var initialAngle = 0.0
    private var cumulativeRotation = 0.0
    private var isRotating = false

    interface OnRotationGestureListener {
        fun onRotationBegin()
        fun onRotation(angle: Float)
        fun onRotationEnd()
    }

    fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (event.pointerCount == 2) {
                    initialAngle = calculateAngle(event)
                    isRotating = true
                    listener.onRotationBegin()
                }
            }
            MotionEvent.ACTION_MOVE -> {
                if (isRotating && event.pointerCount == 2) {
                    val currentAngle = calculateAngle(event)
                    val deltaAngle = (currentAngle - initialAngle).toFloat()
                    cumulativeRotation += deltaAngle
                    initialAngle = currentAngle
                    listener.onRotation(cumulativeRotation.toFloat())
                }
            }
            MotionEvent.ACTION_POINTER_UP -> {
                if (isRotating) {
                    if (event.pointerCount > 1) {
                        // If one pointer is lifted but another remains,
                        // update the initial angle to continue the gesture
                        val remainingPointerIndex = if (event.actionIndex == 0) 1 else 0
                        initialAngle = calculateAngle(event, remainingPointerIndex)
                    }
                }
            }
            MotionEvent.ACTION_UP -> {
                if (isRotating) {
                    isRotating = false
                    cumulativeRotation = 0.0
                    listener.onRotationEnd()
                }
            }
        }
        return true
    }

    /**
     * Resets the detector to its initial state without firing any listener callbacks.
     * Call this from onReset() to handle cases where the gesture is cancelled externally
     * (e.g. ACTION_CANCEL from a parent view intercepting touch), mirroring the V1
     * RotateGestureRecognizer pattern of resetting inner detector state on reset.
     */
    fun reset() {
        isRotating = false
        cumulativeRotation = 0.0
        initialAngle = 0.0
    }

    private fun calculateAngle(event: MotionEvent, pointerIndex1: Int = 0, pointerIndex2: Int = 1): Double {
        val deltaX = (event.getX(pointerIndex2) - event.getX(pointerIndex1)).toDouble()
        val deltaY = (event.getY(pointerIndex2) - event.getY(pointerIndex1)).toDouble()
        return atan2(deltaY, deltaX) // Returns angle in radians
    }
}
