package com.snap.valdi.test.matchers

import android.content.Context
import android.view.View
import android.view.inputmethod.InputMethodManager
import org.hamcrest.Description
import org.hamcrest.TypeSafeMatcher

class ViewHasKeyboardMatcher : TypeSafeMatcher<View>() {
    override fun describeTo(description: Description) {
        description.appendText("view with an active keyboard")
    }

    override fun matchesSafely(view: View): Boolean {
        val inputMethodManager =
            view.context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
        return inputMethodManager?.isActive(view) ?: false
    }
}
