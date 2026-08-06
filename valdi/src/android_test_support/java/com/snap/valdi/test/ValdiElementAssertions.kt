package com.snap.valdi.test

import com.snap.valdi.test.assertions.DoesNotExistElementAssertion
import com.snap.valdi.test.assertions.MatchesDescendantElementAssertion
import com.snap.valdi.test.assertions.MatchesElementAssertion
import org.hamcrest.Matcher

object ValdiElementAssertions {

    /**
     * Asserts that there were no matched node as part of the view node interaction
     */
    @JvmStatic
    fun doesNotExist(): DoesNotExistElementAssertion {
        return DoesNotExistElementAssertion()
    }

    /**
     * Asserts that the matched node as part of the view
     * node interaction matches the given matcher.
     */
    @JvmStatic
    fun matches(matcher: Matcher<ValdiElementWithRootView>): MatchesElementAssertion {
        return MatchesElementAssertion(matcher)
    }

    /**
     * Asserts that the matched node as part of the view node interaction matches
     * the given matcher, or as a descendant that matches the given matcher.
     */
    @JvmStatic
    fun matchesAnyDescendant(matcher: Matcher<ValdiElementWithRootView>): ElementAssertion {
        return MatchesDescendantElementAssertion(matcher)
    }
}
