package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description
import org.hamcrest.Matcher

/**
 * Matches a View which has the given Composer attribute name and value.
*/
internal class ValdiAttributeValueMatcher<T>(
    attributeName: String,
    val attributeValue: Matcher<T>
) : ValdiAttributeValueMatcherBase(attributeName) {

    override fun describeAttributeValue(description: Description) {
        attributeValue.describeTo(description)
    }

    override fun matches(item: ValdiElementWithRootView, attributeValue: Any?): Boolean {
        return this.attributeValue.matches(attributeValue)
    }
}
