package com.snap.valdi.attributes.impl.gestures

import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import com.snap.valdi.views.touches.TapGestureRecognizerListener

/**
 * TapContext is a context object that gets added to a view when an onTap action is attached
 */
class TapContext(private val function: ValdiFunction, private val predicate: ValdiFunction?): TapGestureRecognizerListener {

    override fun onRecognized(gesture: ValdiGestureRecognizer, state: ValdiGestureRecognizerState, x: Int, y: Int, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>) {
        if (state == ValdiGestureRecognizerState.ENDED) {
            ValdiMarshaller.use {
                GestureAttributesUtils.pushGestureParams(it, gesture.view, state, x, y, pointerCount, pointerLocations)
                function.perform(it)
            }
        }
    }

    override fun shouldBegin(gesture: ValdiGestureRecognizer, x: Int, y: Int, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>): Boolean {
        return PredicateUtils.shouldBegin(predicate, gesture, x, y, pointerCount, pointerLocations, 0, timeoutMs = PredicateUtils.GESTURE_PREDICATE_TIMEOUT_MS) { _, _ -> }
    }

}
