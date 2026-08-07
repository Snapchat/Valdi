package com.snap.valdi.test.coordinates

import android.graphics.Rect
import android.view.View

object CoordinatesUtils {

    @JvmStatic
    fun getAbsoluteVisibleRect(view: View, relativeFrame: Rect): Rect {
        val parent = view.parent ?: return Rect()

        val absoluteFrame = Rect(relativeFrame)
        val isVisible = parent.getChildVisibleRect(view, absoluteFrame, null)
        if (!isVisible) {
            absoluteFrame.setEmpty()
            return absoluteFrame
        }

        return absoluteFrame
    }
}
