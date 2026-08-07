package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.utils.ValdiElementSearcher
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeDiagnosingMatcher

class DescendantElementMatcher(private val childMatcher: Matcher<ValdiElementWithRootView>) :
    TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    override fun describeTo(description: Description) {
        description.appendText("with descendant matching ")
        childMatcher.describeTo(description)
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        if (ValdiElementSearcher.search(item, childMatcher) != null) {
            return true
        }

        mismatchDescription.appendText("None of the descendants matched")

        return false
    }
}
