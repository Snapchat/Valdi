package com.snap.valdi.test.adapters

import android.view.View
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.utils.ValdiElementSearcher
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeDiagnosingMatcher

class EspressoViewMatcherToElementMatcher(val viewMatcher: Matcher<View>) :
    TypeSafeDiagnosingMatcher<ValdiElementWithRootView>() {

    override fun describeTo(description: Description) {
        viewMatcher.describeTo(description)
    }

    override fun matchesSafely(item: ValdiElementWithRootView, mismatchDescription: Description): Boolean {
        val view = ValdiElementSearcher.searchViewInstance(item)
        if (view == null) {
            mismatchDescription.appendText("Could not find matching View instance for $item")
            return false
        }

        if (viewMatcher.matches(view)) {
            return true
        }

        viewMatcher.describeMismatch(item.rootView, mismatchDescription)

        return false
    }
}
