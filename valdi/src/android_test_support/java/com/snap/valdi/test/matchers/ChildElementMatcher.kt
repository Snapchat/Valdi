package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeDiagnosingMatcher

class ChildElementMatcher(
    private val childMatcher: Matcher<ValdiElementWithRootView>,
    private val ignoreLayouts: Boolean
) :
    TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    override fun describeTo(description: Description) {
        description.appendText("with child matching ")
        childMatcher.describeTo(description)
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        val children = if (ignoreLayouts) item.viewChildren() else item.children()
        if (children.any(childMatcher::matches)) {
            return true
        }

        mismatchDescription.appendText("None of the ${children.size} children matched")

        return false
    }
}
