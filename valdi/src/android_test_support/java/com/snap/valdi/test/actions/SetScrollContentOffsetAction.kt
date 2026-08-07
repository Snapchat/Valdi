package com.snap.valdi.test.actions

import android.graphics.RectF
import androidx.test.espresso.UiController
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.ElementAction

class SetScrollContentOffsetAction(val contentOffsetPercent: Int, val horizontal: Boolean) : ElementAction {

    override fun getDescription(): String {
        return "set content offset to $contentOffsetPercent% ${if (horizontal) "horizontal" else "vertical"}"
    }

    override fun perform(controller: UiController, item: ValdiElementWithRootView) {
        val rect = RectF()
        item.viewNode.getVisualRelativeFrame(rect)

        val pageSize = if (horizontal) rect.width() else rect.height()
        val contentOffset = item.coordinateResolver.fromPixel(pageSize * (contentOffsetPercent.toDouble() / 100.0))

        item.viewNode.setAttribute(if (horizontal) "contentOffsetX" else "contentOffsetY", contentOffset, false)
    }
}
