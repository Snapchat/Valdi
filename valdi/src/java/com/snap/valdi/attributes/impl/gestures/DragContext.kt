package com.snap.valdi.attributes.impl.gestures

import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.InternedString
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import com.snap.valdi.views.touches.DragGestureRecognizer
import com.snap.valdi.views.touches.DragGestureRecognizerListener

class DragContext(
        private val function: ValdiFunction, private val predicate: ValdiFunction?, private val coordinateResolver: CoordinateResolver): DragGestureRecognizerListener {

    private fun pushAdditionalParams(gesture: DragGestureRecognizer, marshaller: ValdiMarshaller, objectIndex: Int, offsetX: Int, offsetY: Int, velocityX: Float, velocityY: Float) {
        val resolvedOffsetX = ViewUtils.resolveDeltaX(gesture.view, offsetX)
        val resolvedVelocityX = ViewUtils.resolveDeltaX(gesture.view, velocityX.toDouble())

        marshaller.putMapPropertyDouble(deltaXProperty, objectIndex,coordinateResolver.fromPixel(resolvedOffsetX))
        marshaller.putMapPropertyDouble(deltaYProperty, objectIndex, coordinateResolver.fromPixel(offsetY))
        marshaller.putMapPropertyDouble(velocityXProperty, objectIndex, coordinateResolver.fromPixel(resolvedVelocityX))
        marshaller.putMapPropertyDouble(velocityYProperty, objectIndex, coordinateResolver.fromPixel(velocityY))
    }

    override fun onRecognized(gesture: DragGestureRecognizer, state: ValdiGestureRecognizerState, x: Int, y: Int, offsetX: Int, offsetY: Int, velocityX: Float, velocityY: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>) {
        ValdiMarshaller.use {
            val objectIndex = GestureAttributesUtils.pushGestureParams(
                it,
                gesture.view,
                state,
                x,
                y,
                pointerCount,
                pointerLocations,
                4
            )
            pushAdditionalParams(gesture, it, objectIndex, offsetX, offsetY, velocityX, velocityY)

            GestureAttributesUtils.performGestureCallback(function, state, it)
        }
    }

    override fun shouldBegin(gesture: DragGestureRecognizer, x: Int, y: Int, offsetX: Int, offsetY: Int, velocityX: Float, velocityY: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>): Boolean {
        return PredicateUtils.shouldBegin(predicate, gesture, x, y, pointerCount, pointerLocations, 4, timeoutMs = PredicateUtils.GESTURE_PREDICATE_TIMEOUT_MS) { marshaller, objectIndex ->
            pushAdditionalParams(gesture, marshaller, objectIndex, offsetX, offsetY, velocityX, velocityY)
        }
    }

    companion object {
        private val deltaXProperty = InternedString.create("deltaX")
        private val deltaYProperty = InternedString.create("deltaY")
        private val velocityXProperty = InternedString.create("velocityX")
        private val velocityYProperty = InternedString.create("velocityY")
    }
}
