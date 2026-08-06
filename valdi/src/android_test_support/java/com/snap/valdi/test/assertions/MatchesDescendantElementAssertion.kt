package com.snap.valdi.test.assertions

import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.utils.ValdiElementSearcher
import com.snap.valdi.test.utils.assertThat
import org.hamcrest.Matcher
import org.hamcrest.Matchers
import org.hamcrest.StringDescription

class MatchesDescendantElementAssertion(val matcher: Matcher<ValdiElementWithRootView>) :
    NonNullableElementAssertion() {

    private fun matchesAnyDescendant(element: ValdiElementWithRootView): Boolean {
        return ValdiElementSearcher.search(element, matcher) != null
    }

    override fun doCheck(element: ValdiElementWithRootView) {
        assertThat(
            {
                StringDescription()
                    .appendText("'")
                    .also(matcher::describeTo)
                    .appendText("' matches any descendant view node.")
                    .toString()
            },
            matchesAnyDescendant(element), Matchers.`is`(true)
        )
    }
}
