package com.snap.valdi.attributes.impl.gestures

import android.view.View
import android.view.ViewConfiguration
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.views.ValdiRootView
import com.snap.valdi.views.touches.DoubleTapGestureRecognizer
import com.snap.valdi.views.touches.DragGestureRecognizer
import com.snap.valdi.views.touches.GestureRecognizers
import com.snap.valdi.views.touches.LongPressGestureRecognizer
import com.snap.valdi.views.touches.PinchGestureRecognizer
import com.snap.valdi.views.touches.PinchGestureRecognizerV2
import com.snap.valdi.views.touches.RotateGestureRecognizer
import com.snap.valdi.views.touches.RotateGestureRecognizerV2
import com.snap.valdi.views.touches.TapGestureRecognizer
import com.snap.valdi.views.touches.TouchGestureRecognizer

class GestureAttributes(private val coordinateResolver: CoordinateResolver) {

    fun applyHitTest(view: View, hitTest: ValdiFunction) {
        val gestureRecognizer = addGestureRecognizersIfNeeded(view)
        gestureRecognizer.hitTest = hitTest
    }

    fun resetHitTest(view: View) {
        val gestureRecognizers = ViewUtils.getGestureRecognizers(view, createIfNeeded = false) ?: return
        gestureRecognizers.hitTest = null
    }

    fun applyOnTap(view: View, action: ValdiFunction, predicate: ValdiFunction?, additionalValue: Any?) {
        val gestureRecognizer = addGestureRecognizersIfNeeded(view)
        val tapRecognizer = TapGestureRecognizer(view, TapContext(action, predicate))
        gestureRecognizer.addGestureRecognizer(tapRecognizer)
    }

    fun resetOnTap(view: View) {
        removeGestureRecognizerIfNeeded(view, TapGestureRecognizer::class.java)
    }

    fun applyOnDoubleTap(view: View, action: ValdiFunction, predicate: ValdiFunction?, additionalValue: Any?) {
        val gestureRecognizer = addGestureRecognizersIfNeeded(view)
        val doubleTapRecognizer = DoubleTapGestureRecognizer(view, DoubleTapContext(action, predicate))
        gestureRecognizer.addGestureRecognizer(doubleTapRecognizer)
    }

    fun resetOnDoubleTap(view: View) {
        removeGestureRecognizerIfNeeded(view, DoubleTapGestureRecognizer::class.java)
    }

    fun applyOnLongPress(view: View, action: ValdiFunction, predicate: ValdiFunction?, longPressDuration: Any?) {
        val gestureRecognizer = addGestureRecognizersIfNeeded(view)
        val longPressRecognizer = LongPressGestureRecognizer(view, LongPressContext(action, predicate))

        if (longPressDuration is Number) {
            longPressRecognizer.longPressDurationMs = (longPressDuration.toDouble() * 1000.0).toLong()
        }

        gestureRecognizer.addGestureRecognizer(longPressRecognizer)
    }

    fun resetOnLongPress(view: View) {
        removeGestureRecognizerIfNeeded(view, LongPressGestureRecognizer::class.java)
    }

    fun applyOnDrag(view: View, action: ValdiFunction, predicate: ValdiFunction?, additionalValue: Any?) {
        val gestureRecognizer = addGestureRecognizersIfNeeded(view)
        val dragRecognizer = DragGestureRecognizer(view, DragContext(action, predicate, coordinateResolver))
        gestureRecognizer.addGestureRecognizer(dragRecognizer)
    }

    fun resetOnDrag(view: View) {
        removeGestureRecognizerIfNeeded(view, DragGestureRecognizer::class.java)
    }

    fun applyOnPinch(view: View, action: ValdiFunction, predicate: ValdiFunction?, additionalValue: Any?) {
        val gestureRecognizer = addGestureRecognizersIfNeeded(view)
        val pinchRecognizer = if (enablePinchGestureRecognizeV2(view)) {
            PinchGestureRecognizerV2(view, PinchContext(action, predicate), enableV2GestureDetectorReset(view))
        } else {
            PinchGestureRecognizer(view, PinchContext(action, predicate))
        }
        gestureRecognizer.addGestureRecognizer(pinchRecognizer)
    }

    fun resetOnPinch(view: View) {
        removeGestureRecognizerIfNeeded(view, PinchGestureRecognizer::class.java)
    }

    fun applyOnRotate(view: View, action: ValdiFunction, predicate: ValdiFunction?, additionalValue: Any?) {
        val gestureRecognizer = addGestureRecognizersIfNeeded(view)
        val rotateRecognizer = if (enableRotateGestureRecognizeV2(view))
            RotateGestureRecognizerV2(view, RotateContext(action, predicate), enableV2GestureDetectorReset(view))
        else
            RotateGestureRecognizer(view, RotateContext(action, predicate))
        gestureRecognizer.addGestureRecognizer(rotateRecognizer)
    }

    fun resetOnRotate(view: View) {
        if (enableRotateGestureRecognizeV2(view))
            removeGestureRecognizerIfNeeded(view, RotateGestureRecognizerV2::class.java)
        else
            removeGestureRecognizerIfNeeded(view, RotateGestureRecognizer::class.java)
    }

    private fun enableRotateGestureRecognizeV2(view: View): Boolean {
        val rootView = if (view is ValdiRootView) {
            view
        } else {
            ViewUtils.findValdiContext(view)?.rootView
        }
        return rootView?.enableRotateGestureRecognizeV2 ?: false
    }

    private fun enablePinchGestureRecognizeV2(view: View): Boolean {
        val rootView = if (view is ValdiRootView) {
            view
        } else {
            ViewUtils.findValdiContext(view)?.rootView
        }
        return rootView?.enablePinchGestureRecognizeV2 ?: false
    }

    private fun enableV2GestureDetectorReset(view: View): Boolean {
        val rootView = if (view is ValdiRootView) {
            view
        } else {
            ViewUtils.findValdiContext(view)?.rootView
        }
        return rootView?.enableV2GestureDetectorReset ?: false
    }


    fun applyOnTouchStart(view: View, action: ValdiFunction) {
        val touchRecognizer = addTouchRecognizerIfNeeded(view)
        val touchContext = touchRecognizer.listener as? TouchContext
        touchContext?.startAction = action
    }

    fun resetOnTouchStart(view: View) {
        val touchRecognizer = getTouchRecognizer(view)
        val touchContext = touchRecognizer?.listener as? TouchContext
        touchContext?.startAction = null
        removeTouchRecognizerIfNeeded(view)
    }

    fun applyOnTouch(view: View, action: ValdiFunction) {
        val touchRecognizer = addTouchRecognizerIfNeeded(view)
        val touchContext = touchRecognizer.listener as? TouchContext
        touchContext?.action = action
    }

    fun resetOnTouch(view: View) {
        val touchRecognizer = getTouchRecognizer(view)
        val touchContext = touchRecognizer?.listener as? TouchContext
        touchContext?.action = null
        removeTouchRecognizerIfNeeded(view)
    }

    fun applyOnTouchEnd(view: View, action: ValdiFunction) {
        val touchRecognizer = addTouchRecognizerIfNeeded(view)
        val touchContext = touchRecognizer.listener as? TouchContext
        touchContext?.endAction = action
    }

    fun resetOnTouchEnd(view: View) {
        val touchRecognizer = getTouchRecognizer(view)
        val touchContext = touchRecognizer?.listener as? TouchContext
        touchContext?.endAction = null
        removeTouchRecognizerIfNeeded(view)
    }

    private fun addTouchRecognizerIfNeeded(view: View): TouchGestureRecognizer {
        val gestureRecognizer = ViewUtils.getOrCreateGestureRecognizers(view)

        var touchRecognizer = gestureRecognizer.getGestureRecognizer(TouchGestureRecognizer::class.java)
        if (touchRecognizer != null) {
            return touchRecognizer
        }

        touchRecognizer = TouchGestureRecognizer(view, TouchContext())
        gestureRecognizer.addGestureRecognizer(touchRecognizer)

        return touchRecognizer
    }

    private fun getTouchRecognizer(view: View): TouchGestureRecognizer? {
        return ViewUtils.getGestureRecognizers(view, createIfNeeded = false)?.getGestureRecognizer(TouchGestureRecognizer::class.java)
    }

    private fun addGestureRecognizersIfNeeded(view: View): GestureRecognizers {
        return ViewUtils.getOrCreateGestureRecognizers(view)
    }

    private fun removeTouchRecognizerIfNeeded(view: View) {
        val touchRecognizer = getTouchRecognizer(view)
        touchRecognizer ?: return

        val touchContext = touchRecognizer.listener as? TouchContext ?: return
        if (touchContext.action == null && touchContext.startAction == null && touchContext.endAction == null) {
            removeGestureRecognizerIfNeeded(view, touchRecognizer::class.java)
        }
    }

    private fun removeGestureRecognizerIfNeeded(view: View, gestureRecognizer: Class<*>) {
        val gestureRecognizers = ViewUtils.getGestureRecognizers(view, createIfNeeded = false) ?: return
        gestureRecognizers.removeGestureRecognizer(gestureRecognizer)
    }
}
