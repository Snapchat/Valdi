package com.snap.valdi.attributes.impl.gestures

import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.InternedString
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import com.snap.valdi.views.touches.PinchGestureRecognizer
import com.snap.valdi.views.touches.PinchGestureRecognizerListener

class PinchContext(private val function: ValdiFunction, private val predicate: ValdiFunction?): PinchGestureRecognizerListener {

    override fun onRecognized(gesture: ValdiGestureRecognizer, state: ValdiGestureRecognizerState, x: Int, y: Int, scale: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>) {
        ValdiMarshaller.use {
            val objectIndex = GestureAttributesUtils.pushGestureParams(
                it,
                gesture.view,
                state,
                x,
                y,
                pointerCount,
                pointerLocations,
                1
            )
            it.putMapPropertyFloat(scaleProperty, objectIndex, scale)

            GestureAttributesUtils.performGestureCallback(function, state, it)
        }
    }

    override fun shouldBegin(gesture: ValdiGestureRecognizer, x: Int, y: Int, scale: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>): Boolean {
        return PredicateUtils.shouldBegin(predicate, gesture, x, y, pointerCount, pointerLocations, 1, timeoutMs = PredicateUtils.GESTURE_PREDICATE_TIMEOUT_MS) { marshaller, objectIndex ->
            marshaller.putMapPropertyFloat(scaleProperty, objectIndex, scale)
        }
    }

    private companion object {
        val scaleProperty = InternedString.create("scale")
    }

}
