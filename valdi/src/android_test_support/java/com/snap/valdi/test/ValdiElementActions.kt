package com.snap.valdi.test

import android.view.InputDevice
import android.view.MotionEvent
import androidx.test.espresso.action.GeneralClickAction
import androidx.test.espresso.action.GeneralSwipeAction
import androidx.test.espresso.action.Press
import androidx.test.espresso.action.Swipe
import androidx.test.espresso.action.Tap
import com.snap.valdi.test.actions.ElementActionAdapter
import com.snap.valdi.test.actions.ElementEditorAction
import com.snap.valdi.test.actions.ElementTypeTextAction
import com.snap.valdi.test.actions.ScrollToChildAction
import com.snap.valdi.test.actions.SetAttributeAction
import com.snap.valdi.test.actions.SetScrollContentOffsetAction
import com.snap.valdi.test.actions.WaitUntilScrollEndAction
import org.hamcrest.Matcher

object ValdiElementActions {

    private const val EDGE_FUZZ_FACTOR: Float = 0.083f
    private const val DEFAULT_SCROLL_END_TIMEOUT_MS = 10000L

    /**
     * Performs a click in the visible area of the matched node
     */
    @JvmStatic
    fun click(inputDevice: Int, buttonState: Int): ElementAction {
        return ElementActionAdapter("click") {
            GeneralClickAction(
                Tap.SINGLE,
                it.limitingToVisibleParts().atHorizontalCenter().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER,
                inputDevice,
                buttonState
            )
        }
    }

    /**
     * Performs a click in the visible area of the matched node
     */
    @JvmStatic
    fun click(): ElementAction {
        return ElementActionAdapter("click") {
            GeneralClickAction(
                Tap.SINGLE,
                it.limitingToVisibleParts().atHorizontalCenter().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER,
                InputDevice.SOURCE_UNKNOWN,
                MotionEvent.BUTTON_PRIMARY
            )
        }
    }

    /**
     * Performs a click in the visible area of the matched node
     */
    @JvmStatic
    fun clickOnStartOfElement(): ElementAction {
        return ElementActionAdapter("click") {
            GeneralClickAction(
                Tap.SINGLE,
                it.limitingToVisibleParts().atHorizontalStart().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER,
                InputDevice.SOURCE_UNKNOWN,
                MotionEvent.BUTTON_PRIMARY
            )
        }
    }

    /**
     * Performs a click near the bottom-left of the matched node.
     * Useful for tapping background areas while avoiding top content.
     */
    @JvmStatic
    fun clickOnBottomStartOfElement(
        horizontalOffsetRatio: Float = EDGE_FUZZ_FACTOR,
        verticalOffsetRatio: Float = -EDGE_FUZZ_FACTOR
    ): ElementAction {
        return ElementActionAdapter("click bottom-start") {
            GeneralClickAction(
                Tap.SINGLE,
                it.limitingToVisibleParts()
                    .atHorizontalStart()
                    .atVerticalEnd()
                    .atOffsetRatio(horizontalOffsetRatio, verticalOffsetRatio)
                    .toCoordinatesProvider(),
                Press.FINGER,
                InputDevice.SOURCE_UNKNOWN,
                MotionEvent.BUTTON_PRIMARY
            )
        }
    }

    /**
     * Performs a swipe left in the area of the matched node
     */
    @JvmStatic
    fun swipeLeft(): ElementAction {
        return ElementActionAdapter("swipe left") {
            GeneralSwipeAction(
                Swipe.FAST,
                it.atHorizontalEnd().atVerticalCenter().atOffsetRatio(-EDGE_FUZZ_FACTOR, 0.0f).toCoordinatesProvider(),
                it.atHorizontalStart().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /**
     * Performs a slow swipe left in the area of the matched node
     */
    @JvmStatic
    fun slowSwipeLeft(): ElementAction {
        return ElementActionAdapter("slow swipe left") {
            GeneralSwipeAction(
                Swipe.SLOW,
                it.atHorizontalEnd().atVerticalCenter().atOffsetRatio(-EDGE_FUZZ_FACTOR, 0.0f).toCoordinatesProvider(),
                it.atHorizontalStart().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /**
     * Performs a swipe left starting from the center of the matched node (center -> left edge).
     * Use this when the element is a Valdi element; for View matchers use crema Actions.swipeLeftFromCenter().
     */
    @JvmStatic
    fun swipeLeftFromCenter(): ElementAction {
        return ElementActionAdapter("swipe left from center") {
            GeneralSwipeAction(
                Swipe.FAST,
                it.limitingToVisibleParts().atHorizontalCenter().atVerticalCenter().toCoordinatesProvider(),
                it.atHorizontalStart().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /**
     * Performs a swipe right in the area of the matched node
     */
    @JvmStatic
    fun swipeRight(): ElementAction {
        return ElementActionAdapter("swipe right") {
            GeneralSwipeAction(
                Swipe.FAST,
                it.atHorizontalStart().atVerticalCenter().atOffsetRatio(EDGE_FUZZ_FACTOR, 0.0f).toCoordinatesProvider(),
                it.atHorizontalEnd().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /**
     * Performs a slow swipe right in the area of the matched node
     */
    @JvmStatic
    fun slowSwipeRight(): ElementAction {
        return ElementActionAdapter("slow swipe right") {
            GeneralSwipeAction(
                Swipe.SLOW,
                it.atHorizontalStart().atVerticalCenter().atOffsetRatio(EDGE_FUZZ_FACTOR, 0.0f).toCoordinatesProvider(),
                it.atHorizontalEnd().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /**
     * Performs a swipe down in the area of the matched node
     */
    @JvmStatic
    fun swipeDown(): ElementAction {
        return ElementActionAdapter("swipe down") {
            GeneralSwipeAction(
                Swipe.FAST,
                it.atHorizontalCenter().atVerticalStart().atOffsetRatio(0.0f, EDGE_FUZZ_FACTOR).toCoordinatesProvider(),
                it.atHorizontalCenter().atVerticalEnd().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /**
     * Performs a slow swipe down in the area of the matched node
     */
    @JvmStatic
    fun slowSwipeDown(): ElementAction {
        return ElementActionAdapter("slow swipe down") {
            GeneralSwipeAction(
                Swipe.SLOW,
                it.atHorizontalCenter().atVerticalStart().atOffsetRatio(0.0f, EDGE_FUZZ_FACTOR).toCoordinatesProvider(),
                it.atHorizontalCenter().atVerticalEnd().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /**
     * Performs a swipe up in the area of the matched node
     */
    @JvmStatic
    fun swipeUp(): ElementAction {
        return ElementActionAdapter("swipe up") {
            GeneralSwipeAction(
                Swipe.FAST,
                it.atHorizontalCenter().atVerticalEnd().atOffsetRatio(0.0f, -EDGE_FUZZ_FACTOR).toCoordinatesProvider(),
                it.atHorizontalCenter().atVerticalStart().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /** Scrolls the Valdi scroll view to a child element matching the given matcher. */
    @JvmStatic
    fun scrollToChild(
        childMatcher: Matcher<ValdiElementWithRootView>,
        maxScrollDistance: Float = 20f,
        scrollIncrement: Float = 1f
    ): ElementAction {
        return ScrollToChildAction(childMatcher, maxScrollDistance, scrollIncrement)
    }

    /**
     * Performs a slow swipe up in the area of the matched node
     */
    @JvmStatic
    fun slowSwipeUp(): ElementAction {
        return ElementActionAdapter("slow swipe up") {
            GeneralSwipeAction(
                Swipe.SLOW,
                it.atHorizontalCenter().atVerticalEnd().atOffsetRatio(0.0f, -EDGE_FUZZ_FACTOR).toCoordinatesProvider(),
                it.atHorizontalCenter().atVerticalStart().toCoordinatesProvider(),
                Press.FINGER
            )
        }
    }

    /**
     * Performs a double click in the area of the matched node
     */
    @JvmStatic
    fun doubleClick(): ElementAction {
        return ElementActionAdapter("double click") {
            GeneralClickAction(
                Tap.DOUBLE,
                it.atHorizontalCenter().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER,
                InputDevice.SOURCE_UNKNOWN,
                MotionEvent.BUTTON_PRIMARY
            )
        }
    }

    /**
     * Performs a long click in the area of the matched node
     */
    @JvmStatic
    fun longClick(): ElementAction {
        return ElementActionAdapter("long click") {
            GeneralClickAction(
                Tap.LONG,
                it.atHorizontalCenter().atVerticalCenter().toCoordinatesProvider(),
                Press.FINGER,
                InputDevice.SOURCE_UNKNOWN,
                MotionEvent.BUTTON_PRIMARY
            )
        }
    }

    /**
     * Wait until scroll has settled on the matched node.
     * This action will only work when the matched node is a scroll element.
     */
    @JvmStatic
    fun waitUntilScrollEnd(timeoutMs: Long = DEFAULT_SCROLL_END_TIMEOUT_MS): ElementAction {
        return WaitUntilScrollEndAction(timeoutMs)
    }

    /**
     * Set a Composer attribute on the matched node.
     * This can be used to modify the view node in tests.
     */
    @JvmStatic
    fun setAttribute(attributeName: String, attributeValue: Any?): ElementAction {
        return SetAttributeAction(attributeName, attributeValue)
    }

    /**
     * Set the horizontal scroll percentage on the matched node.
     * This action will only work when the matched node is a scroll element.
     * The scroll offset percent is defined as the ratio in percent between
     * the scroll offset and the scroll element size. The percent will be at
     * 0% is when the scroll element is at the beginning (contentOffset of 0).
     * It will be at 100% is when it has been scrolled by a whole "page",
     * meaning if the scroll element is 400 pts, at that its content is at 10000 pts,
     * it will be at 50% when content offset is at 200pts (half the scroll element size),
     * 100% at 400pts, 200% at 800pts etc... Therefore, 100% represent one whole page
     * scrolled, 200% represent 2 pages.
     */
    @JvmStatic
    fun setHorizontalScrollOffset(horizontalScrollOffsetPercent: Int): ElementAction {
        return SetScrollContentOffsetAction(horizontalScrollOffsetPercent, true)
    }

    /**
     * Set the vertical scroll percentage on the matched node.
     * This action will only work when the matched node is a scroll element.
     * The scroll offset percent is defined as the ratio in percent between
     * the scroll offset and the scroll element size. The percent will be at
     * 0% is when the scroll element is at the beginning (contentOffset of 0).
     * It will be at 100% is when it has been scrolled by a whole "page",
     * meaning if the scroll element is 400 pts, at that its content is at 10000 pts,
     * it will be at 50% when content offset is at 200pts (half the scroll element size),
     * 100% at 400pts, 200% at 800pts etc... Therefore, 100% represent one whole page
     * scrolled, 200% represent 2 pages.
     */
    @JvmStatic
    fun setVerticalScrollOffset(verticalScrollOffsetPercent: Int): ElementAction {
        return SetScrollContentOffsetAction(verticalScrollOffsetPercent, false)
    }

    /**
     * Returns an action that selects the view (by clicking on it) and types the provided string into
     * the view. Appending a \n to the end of the string translates to a ENTER key event. Note: this
     * method performs a tap on the view before typing to force the view into focus, if the view
     * already contains text this tap may place the cursor at an arbitrary position within the text.
     * <br>
     * <br>
     * View preconditions:
     *
     * <ul>
     *   <li>must be displayed on screen
     *   <li>must support input methods
     * </ul>
     */
    @JvmStatic
    fun typeText(text: String): ElementAction {
        return ElementTypeTextAction(text, click())
    }

    /**
     * Returns an action that types the provided string into the view. Appending a \n to the end of
     * the string translates to a ENTER key event. Note: this method does not change cursor position
     * in the focused view - text is inserted at the location where the cursor is currently pointed.
     * <br>
     * <br>
     * View preconditions:
     *
     * <ul>
     *   <li>must be displayed on screen
     *   <li>must support input methods
     *   <li>must be already focused
     * </ul>
     */
    @JvmStatic
    fun typeTextIntoFocusedElement(text: String): ElementAction {
        return ElementTypeTextAction(text, null)
    }

    @JvmStatic
    fun pressImeActionButton(): ElementAction {
        return ElementEditorAction()
    }
}
