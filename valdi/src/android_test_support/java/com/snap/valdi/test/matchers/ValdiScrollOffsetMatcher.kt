package com.snap.valdi.test.matchers

import android.graphics.RectF
import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description
import org.hamcrest.TypeSafeDiagnosingMatcher
import kotlin.math.roundToInt

class ValdiScrollOffsetMatcher(
    private val minContentOffsetPercent: Int,
    private val maxContentOffsetPercent: Int,
    private val horizontal: Boolean
) : TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    override fun describeTo(description: Description) {
        description.appendText(
            "with Scroll Content Offset Percent between $minContentOffsetPercent% and $maxContentOffsetPercent%"
        )
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        val attributeName = if (horizontal) "contentOffsetX" else "contentOffsetY"
        val contentOffset = item.viewNode.getAttribute(attributeName) as? Double ?: 0.0
        val contentOffsetPixels = item.coordinateResolver.toPixelF(contentOffset)

        val rect = RectF()
        item.viewNode.getVisualRelativeFrame(rect)

        val pageSize = if (horizontal) rect.width() else rect.height()

        val scrollPercent = ((contentOffsetPixels / pageSize) * 100.0).roundToInt()

        if (scrollPercent < minContentOffsetPercent || scrollPercent > maxContentOffsetPercent) {
            mismatchDescription.appendText("Scroll Content Offset Percent is at $scrollPercent%")
            return false
        }

        return true
    }
}
