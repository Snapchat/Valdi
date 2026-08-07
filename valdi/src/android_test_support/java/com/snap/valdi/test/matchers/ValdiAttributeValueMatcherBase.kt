package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description
import org.hamcrest.TypeSafeDiagnosingMatcher

internal abstract class ValdiAttributeValueMatcherBase(
    val attributeName: String
) : TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    protected abstract fun describeAttributeValue(description: Description)
    protected abstract fun matches(item: ValdiElementWithRootView, attributeValue: Any?): Boolean

    final override fun describeTo(description: Description) {
        description.appendText("with Composer attribute name '$attributeName' and value '")
        describeAttributeValue(description)
        description.appendText("'")
    }

    final override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        val existingValue = item.viewNode.getAttribute(attributeName)

        if (matches(item, existingValue)) {
            return true
        }

        mismatchDescription.appendText("Attribute '$attributeName' was $existingValue")

        return false
    }
}
