package com.snap.valdi.test.matchers

import android.view.View
import com.snap.valdi.views.ValdiRootView
import org.hamcrest.Description
import org.hamcrest.TypeSafeMatcher

/**
 * Matches any Root View inflated from Composer
*/
internal class ValdiRootViewMatcher : TypeSafeMatcher<View>() {

    override fun describeTo(description: Description) {
        description.appendText("with ValdiRootView")
    }

    override fun matchesSafely(item: View): Boolean {
        return item is ValdiRootView
    }
}
