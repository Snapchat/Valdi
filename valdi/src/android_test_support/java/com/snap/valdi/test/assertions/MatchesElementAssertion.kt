package com.snap.valdi.test.assertions

import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.utils.assertThat
import org.hamcrest.Matcher
import org.hamcrest.StringDescription

class MatchesElementAssertion(val matcher: Matcher<ValdiElementWithRootView>) : NonNullableElementAssertion() {
    override fun doCheck(element: ValdiElementWithRootView) {
        assertThat(
            {
                StringDescription()
                    .appendText("'")
                    .also(matcher::describeTo)
                    .appendText("' matches the selected view node")
                    .toString()
            },
            element, matcher
        )
    }
}
