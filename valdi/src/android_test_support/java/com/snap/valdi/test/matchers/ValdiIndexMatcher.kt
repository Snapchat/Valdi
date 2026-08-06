package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeDiagnosingMatcher

internal class ValdiIndexMatcher(
    private val index: Int,
    private val matcher: Matcher<ValdiElementWithRootView>
) : TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {
    private var currentIndex = 0
    override fun describeTo(description: Description) {
        description.appendText("with index: ")
        description.appendValue(index)
        matcher.describeTo(description)
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        return matcher.matches(item) && currentIndex++ == index
    }
}
