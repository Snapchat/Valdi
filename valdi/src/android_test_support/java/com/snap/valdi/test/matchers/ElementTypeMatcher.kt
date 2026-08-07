package com.snap.valdi.test.matchers

import com.snap.valdi.test.ValdiElementWithRootView
import org.hamcrest.Description
import org.hamcrest.TypeSafeDiagnosingMatcher

class ElementTypeMatcher(val elementType: String) : TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    override fun describeTo(description: Description) {
        description.appendText("with Composer Element Type '$elementType'")
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        val viewClassName = item.viewNode.viewClassName
        val resolvedElementType = elementTypeMapping[viewClassName] ?: viewClassName

        if (elementType == resolvedElementType) {
            return true
        }

        mismatchDescription.appendText("Element Type was '$resolvedElementType'")

        return false
    }

    companion object {
        // TODO(simon): Remove after introducing the elementType property in ValdiViewNode
        private val elementTypeMapping = hashMapOf<String, String>(
            "com.snap.valdi.views.ValdiView" to "view",
            "com.snap.valdi.views.ValdiTextView" to "label",
            "com.snap.valdi.views.ValdiScrollView" to "scroll",
            "com.snap.valdi.views.ValdiImageView" to "image",
            "com.snap.valdi.views.ValdiSpinnerView" to "spinner",
            "com.snap.valdi.views.ValdiEditText" to "textfield",
            "com.snap.valdi.views.ValdiEditTextMultiline" to "textview",
            "com.snap.valdi.views.ShapeView" to "shape"
        )
    }
}
