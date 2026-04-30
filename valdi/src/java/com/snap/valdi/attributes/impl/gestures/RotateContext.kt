package com.snap.valdi.attributes.impl.gestures

import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.InternedString
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import com.snap.valdi.views.touches.RotateGestureRecognizerListener

class RotateContext(private val function: ValdiFunction, private val predicate: ValdiFunction?): RotateGestureRecognizerListener {

    override fun onRecognized(gesture: ValdiGestureRecognizer, state: ValdiGestureRecognizerState, x: Int, y: Int, rotation: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>) {
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
            val resolvedRotation = ViewUtils.resolveDeltaX(gesture.view, rotation)
            it.putMapPropertyFloat(rotationProperty, objectIndex, resolvedRotation)

            GestureAttributesUtils.performGestureCallback(function, state, it)
        }
    }

    override fun shouldBegin(gesture: ValdiGestureRecognizer, x: Int, y: Int, rotation: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>): Boolean {
        return PredicateUtils.shouldBegin(predicate, gesture, x, y, pointerCount, pointerLocations, 1, timeoutMs = PredicateUtils.GESTURE_PREDICATE_TIMEOUT_MS) { marshaller, objectIndex ->
            marshaller.putMapPropertyFloat(rotationProperty, objectIndex, rotation)
        }
    }

    private companion object {
        val rotationProperty = InternedString.create("rotation")
    }
}
