package com.snap.valdi.test.actions

import android.graphics.RectF
import androidx.test.espresso.PerformException
import androidx.test.espresso.UiController
import com.snap.valdi.test.ElementAction
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.utils.ValdiElementSearcher
import org.hamcrest.Matcher

/**
 * Scrolls a Valdi scroll view to find a child element matching the given matcher.
 * 
 * Uses programmatic scrolling by setting the contentOffsetY attribute directly,
 * which is more reliable on simulators than swipe gestures.
 *
 * Note: Unlike RecyclerViewActions.scrollTo which can find items in the adapter
 * before scrolling, Valdi only exposes visible children via getVisibleChildren().
 * Therefore, this action must incrementally scroll and search until the target
 * element becomes visible.
 *
 * @param childMatcher The matcher to identify the target child element to scroll to.
 * @param maxScrollDistance Maximum distance to scroll, expressed as a multiple of the 
 *        viewport height. For example, 20.0 means scroll up to 20 viewport heights 
 *        (20 "pages") before giving up. Default is 20.0.
 * @param scrollIncrement Distance to scroll per iteration, expressed as a multiple of 
 *        the viewport height. For example, 1.0 means scroll one full viewport height 
 *        per iteration (one "page"). Default is 1.0.
 */
class ScrollToChildAction(
    private val childMatcher: Matcher<ValdiElementWithRootView>,
    private val maxScrollDistance: Float = 20f,
    private val scrollIncrement: Float = 1f
) : ElementAction {

    override fun getDescription(): String {
        return "scroll to child matching: $childMatcher"
    }

    override fun perform(controller: UiController, item: ValdiElementWithRootView) {
        val rect = RectF()
        item.viewNode.getVisualRelativeFrame(rect)
        val viewportHeight = rect.height()

        var currentScrollDistance = 0f

        while (currentScrollDistance <= maxScrollDistance) {
            // Check if child is visible
            val matchingChild = findMatchingChild(item)
            if (matchingChild != null) {
                // Found the child, scroll it into view if needed
                scrollChildIntoView(controller, item, matchingChild, viewportHeight)
                return
            }

            // Scroll down by incrementing content offset
            currentScrollDistance += scrollIncrement
            val contentOffset = item.coordinateResolver.fromPixel(
                (viewportHeight * currentScrollDistance).toDouble()
            )
            item.viewNode.setAttribute("contentOffsetY", contentOffset, false)
            controller.loopMainThreadForAtLeast(100L)
        }

        throw PerformException.Builder()
            .withActionDescription(getDescription())
            .withViewDescription(item.toString())
            .withCause(RuntimeException("Could not find child matching: $childMatcher after scrolling ${maxScrollDistance}x viewport height"))
            .build()
    }

    private fun findMatchingChild(item: ValdiElementWithRootView): ValdiElementWithRootView? {
        return try {
            ValdiElementSearcher.searchFromRootView(item.rootView, childMatcher)
        } catch (e: Exception) {
            null
        }
    }

    private fun scrollChildIntoView(
        controller: UiController,
        scrollView: ValdiElementWithRootView,
        child: ValdiElementWithRootView,
        viewportHeight: Float
    ) {
        val childRect = RectF()
        child.viewNode.getVisualRelativeFrame(childRect)

        val scrollRect = RectF()
        scrollView.viewNode.getVisualRelativeFrame(scrollRect)

        // Check if child is within the visible area of the scroll view
        if (childRect.top >= scrollRect.top && childRect.bottom <= scrollRect.bottom) {
            // Already fully visible
            return
        }

        // Calculate scroll offset needed to bring child into view
        val currentOffsetY = scrollView.viewNode.getAttribute("contentOffsetY") as? Double ?: 0.0
        val currentOffsetPixels = scrollView.coordinateResolver.toPixelF(currentOffsetY)

        // Scroll so the child is visible with some padding (10% of viewport)
        val targetOffsetPixels = childRect.top - scrollRect.top + currentOffsetPixels - (viewportHeight * 0.1f)
        val targetOffset = scrollView.coordinateResolver.fromPixel(targetOffsetPixels.toDouble())

        scrollView.viewNode.setAttribute("contentOffsetY", targetOffset, false)
        controller.loopMainThreadForAtLeast(100L)
    }
}

