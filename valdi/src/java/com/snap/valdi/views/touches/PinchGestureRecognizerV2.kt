package com.snap.valdi.views.touches

import android.view.MotionEvent
import android.view.View

class PinchGestureRecognizerV2 (
        view: View,
        val listener: PinchGestureRecognizerListener,
        val resetDetector: Boolean = false
) : ValdiGestureRecognizer(view){

    var scale = 1.0f
    val pinchDetector = PinchGestureDetector(view.context, object : PinchGestureDetector.PinchGestureDetectorListener {
        override fun onScale(scaleFactor: Float) {
            scale = scaleFactor
        }

        override fun onScaleBegin() {
            if (state == ValdiGestureRecognizerState.POSSIBLE) {
                updateState(ValdiGestureRecognizerState.BEGAN)
            }
        }

        override fun onScaleEnd() {
            updateState(ValdiGestureRecognizerState.ENDED)
        }
    })

    override fun shouldBegin(): Boolean {
        return listener.shouldBegin(this, x, y, scale, pointerCount, pointerLocations)
    }

    override fun onUpdate(event: MotionEvent) {
        pinchDetector.onTouchEvent(event)
    }

    override fun onProcess() {
        listener.onRecognized(this, state, x, y, scale, pointerCount, pointerLocations)
    }

    override fun onReset(event: MotionEvent) {
        super.onReset(event)
        scale = 1.0f
        if (resetDetector) {
            // Reset the inner detector state to avoid spurious onScaleEnd() callbacks on the next
            // gesture sequence. This mirrors the V1 PinchGestureRecognizer pattern where
            // gestureDetector.onTouchEvent(event) is called in onReset() to flush detector state.
            pinchDetector.reset()
        }
    }


    override fun canRecognizeSimultaneously(other: ValdiGestureRecognizer): Boolean {
        return other.javaClass == DragGestureRecognizer::class.java ||
                other.javaClass == RotateGestureRecognizer::class.java ||
                other.javaClass == RotateGestureRecognizerV2::class.java ||
                other.javaClass == ScrollViewDragGestureRecognizer::class.java
    }
}
