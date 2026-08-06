package com.snap.valdi.test.adapters

import android.view.View
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.utils.CapturedValdiElementWithRootView
import com.snap.valdi.test.utils.ValdiElementSearcher
import com.snap.valdi.views.ValdiRootView
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeMatcher

/**
 * Espresso View Matcher implementation that matches on a ValdiRootView that has a
 * node matching the given matcher.
 */
class EspressoViewMatcherAdapter(
    val capturedElement: CapturedValdiElementWithRootView,
    val elementMatcher: Matcher<ValdiElementWithRootView>
) : TypeSafeMatcher<View>() {

    override fun describeTo(description: Description) {
        elementMatcher.describeTo(description)
    }

    override fun matchesSafely(view: View): Boolean {
        if (view is ValdiRootView) {
            val element = ValdiElementSearcher.searchFromRootView(view, elementMatcher)

            if (element != null) {
                capturedElement.set(view, element)
                return true
            }
        }

        return false
    }
}
