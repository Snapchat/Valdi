package com.snap.valdi.views

import android.graphics.Rect
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import com.snap.valdi.views.touches.TouchDispatcher
import com.snap.valdi.views.touches.DragGestureRecognizer

class ValdiScrollUtil {
    private val outLocation = IntArray(2)
    private val rect = Rect()

    // Find the first possible drag gesture candidate
    fun possibleViewDragGesture(view: View, event: MotionEvent, corners: FloatArray = FloatArray(8)): DragGestureRecognizer? {
        if (!TouchDispatcher.hitTest(view, event, outLocation, rect, corners)) {
            return null
        }

        if (view is ValdiView) {
            val gesture = view.getDragGestureRecognizer()
            if (gesture != null) {
                return gesture
            }
        }

        if (view !is ViewGroup) {
            return null;
        }

        for (i in 0 until view.childCount) {
            val dragGesture = possibleViewDragGesture(view.getChildAt(i), event, corners)
            if (dragGesture != null) {
                return dragGesture
            }
        }
        return null;
    }

    fun allPossibleViewDragGestures(view: View, event: MotionEvent, gestures: MutableList<DragGestureRecognizer> = mutableListOf(), corners: FloatArray = FloatArray(8)): List<DragGestureRecognizer> {

        if (!TouchDispatcher.hitTest(view, event, outLocation, rect, corners)) {
            return gestures;
        }

        if (view is ValdiView) {
            val gesture = view.getDragGestureRecognizer()
            if (gesture != null) {
                gestures.add(gesture)
            }
        }

        if (view !is ViewGroup) {
            return gestures;
        }

        for (i in 0 until view.childCount) {
            allPossibleViewDragGestures(view.getChildAt(i), event, gestures, corners)
        }

        return gestures;
    }

    fun canViewScroll(view: View, event: MotionEvent, check: (view: View) -> Boolean, corners: FloatArray = FloatArray(8)): Boolean {
        if (!TouchDispatcher.hitTest(view, event, outLocation, rect, corners)) {
            return false
        }

        if (check(view)) {
            return true
        }

        if (view is ValdiView && view.hasDragGestureRecognizer()) {
            return true
        }

        if (view !is ViewGroup) {
            return false
        }

        for (i in 0 until view.childCount) {
            if (canViewScroll(view.getChildAt(i), event, check, corners)) {
                return true
            }
        }

        return false
    }
}