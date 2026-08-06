package com.snap.valdi.test.matchers

import android.content.Context
import android.util.DisplayMetrics
import android.view.WindowManager
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.coordinates.CoordinatesUtils
import org.hamcrest.Description
import org.hamcrest.TypeSafeDiagnosingMatcher

internal class VisibilityPercentageViewNodeMatcher(
    val minVisibilityPercent: Int,
    val maxVisibilityPercent: Int
) : TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    override fun describeTo(description: Description) {
        description.appendText(
            "View node has effective visibility between $minVisibilityPercent% and $maxVisibilityPercent%"
        )
    }

    private fun computeVisibilityPercent(item: ValdiElementWithRootView): Int {
        val relativeFrame = item.getVisualFrameFromRootView()
        val absoluteFrame = CoordinatesUtils.getAbsoluteVisibleRect(item.rootView, relativeFrame)

        if (absoluteFrame.isEmpty) {
            return 0
        }

        val display = (item.rootView.context.getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay
        val metrics = DisplayMetrics()
        display.getMetrics(metrics)

        val maxViewWidth = Math.min(metrics.widthPixels, relativeFrame.width())
        val maxViewHeight = Math.min(metrics.heightPixels, relativeFrame.height())

        val area = maxViewWidth * maxViewHeight
        val visibleArea = absoluteFrame.width() * absoluteFrame.height()

        val visiblePercent = ((visibleArea.toDouble() / area.toDouble()) * 100.0).toInt()

        return visiblePercent
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        val visibilityPercent = computeVisibilityPercent(item)

        if (visibilityPercent < minVisibilityPercent || visibilityPercent > maxVisibilityPercent) {
            mismatchDescription.appendText("View node has effective visibility at $visibilityPercent%")
            return false
        }

        return true
    }
}
