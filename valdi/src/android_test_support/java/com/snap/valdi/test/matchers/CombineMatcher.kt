package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeDiagnosingMatcher

class CombineMatcher(val matchers: Array<Matcher<ValdiElementWithRootView>>) :
    TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    override fun describeTo(description: Description) {
        var first = true
        for (matcher in matchers) {
            if (!first) {
                description.appendText(" AND ")
            }
            first = false
            description.appendDescriptionOf(matcher)
        }
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        for (matcher in matchers) {
            if (!matcher.matches(item)) {
                matcher.describeMismatch(item, mismatchDescription)
                return false
            }
        }

        return true
    }
}
