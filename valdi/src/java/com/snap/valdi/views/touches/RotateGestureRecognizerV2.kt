package com.snap.valdi.views.touches

import android.view.MotionEvent
import android.view.View

class RotateGestureRecognizerV2(
        view: View,
        val listener: RotateGestureRecognizerListener,
        val resetDetector: Boolean = false
) : ValdiGestureRecognizer(view){

    var rotation = 0.0f

    val rotationDetector = RotationGestureDetector(object : RotationGestureDetector.OnRotationGestureListener {
        override fun onRotationBegin() {
            if (state == ValdiGestureRecognizerState.POSSIBLE) {
                updateState(ValdiGestureRecognizerState.BEGAN)
            }
        }

        override fun onRotation(angle: Float) {
            rotation = angle
        }

        override fun onRotationEnd() {
            updateState(ValdiGestureRecognizerState.ENDED)
        }
    })

    override fun shouldBegin(): Boolean {
        return listener.shouldBegin(this, x, y, rotation, pointerCount, pointerLocations)
    }

    override fun onUpdate(event: MotionEvent) {
        rotationDetector.onTouchEvent(event)
    }

    override fun onProcess() {
        listener.onRecognized(this, state, x, y, rotation, pointerCount, pointerLocations)
    }

    override fun onReset(event: MotionEvent) {
        super.onReset(event)
        rotation = 0.0f
        if (resetDetector) {
            // Reset the inner detector state to avoid spurious onRotationEnd() callbacks on the next
            // gesture sequence. This mirrors the V1 RotateGestureRecognizer pattern where
            // gestureDetector.onTouchEvent(event) is called in onReset() to flush detector state.
            rotationDetector.reset()
        }

    }

    override fun canRecognizeSimultaneously(other: ValdiGestureRecognizer): Boolean {
        return other.javaClass == DragGestureRecognizer::class.java ||
                other.javaClass == PinchGestureRecognizer::class.java ||
                other.javaClass == PinchGestureRecognizerV2::class.java
    }
}
