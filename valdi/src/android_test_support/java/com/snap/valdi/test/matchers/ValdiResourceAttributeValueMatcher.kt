package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description

internal class ValdiResourceAttributeValueMatcher(
    attributeName: String,
    val resId: Int
) : ValdiAttributeValueMatcherBase(attributeName) {

    override fun describeAttributeValue(description: Description) {
        description.appendText("resId $resId")
    }

    override fun matches(item: ValdiElementWithRootView, attributeValue: Any?): Boolean {
        val str = item.rootView.resources.getString(resId)
        return attributeValue == str
    }
}
