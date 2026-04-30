package com.snap.valdi.attributes.impl.gestures

import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import com.snap.valdi.views.touches.LongPressGestureRecognizer
import com.snap.valdi.views.touches.LongPressGestureRecognizerListener

class LongPressContext(private val function: ValdiFunction, private val predicate: ValdiFunction?): LongPressGestureRecognizerListener {

    override fun onRecognized(gesture: LongPressGestureRecognizer, state: ValdiGestureRecognizerState, x: Int, y: Int, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>) {
        ValdiMarshaller.use {
            GestureAttributesUtils.pushGestureParams(it, gesture.view, state, x, y, pointerCount, pointerLocations)
            function.perform(it)
        }
    }

    override fun shouldBegin(gesture: LongPressGestureRecognizer, x: Int, y: Int, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>): Boolean {
        return PredicateUtils.shouldBegin(predicate, gesture, x, y, pointerCount, pointerLocations, 0, timeoutMs = PredicateUtils.GESTURE_PREDICATE_TIMEOUT_MS) { _, _ -> }
    }


}
