package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeDiagnosingMatcher

class AncestorElementMatcher(
    private val parentMatcher: Matcher<ValdiElementWithRootView>,
    private val maxDepth: Int,
    private val ignoreLayouts: Boolean
) : TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    override fun describeTo(description: Description) {
        description.appendText("with ancestor matching ")
        parentMatcher.describeTo(description)
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        var currentDepth = 0
        var current = if (ignoreLayouts) item.viewParent else item.parent
        while (currentDepth < maxDepth && current != null) {
            currentDepth++

            if (parentMatcher.matches(current)) {
                return true
            }

            current = if (ignoreLayouts) current.viewParent else current.parent
        }

        mismatchDescription.appendText(
            "None of the ancestors matched after crawling " +
                "$currentDepth parents with max depth $maxDepth"
        )

        return false
    }
}
