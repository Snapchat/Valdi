package com.snap.valdi.test.adapters

import android.view.View
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.views.ValdiRootView
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeDiagnosingMatcher

/**
 * View matcher that implements compatibility between the deprecated ValdiViewMatchers API
 * and the ValdiViewNodeMatchers API. It works only when using the Composer Android render backend.
 * Unlike "EspressoViewMatcherAdapter", "ValdiLegacyViewMatcherAdapter" matches on view elements that
 * form the Composer UI, instead of matching on a ValdiRootView and one of its child view node.
 */
internal class ValdiLegacyViewMatcherAdapter(private val innerMatcher: Matcher<ValdiElementWithRootView>) :
    TypeSafeDiagnosingMatcher<View>() {

    override fun describeTo(description: Description) {
        innerMatcher.describeTo(description)
    }

    private fun findRootView(item: View): ValdiRootView? {
        var current: View? = item

        while (current != null) {
            if (current is ValdiRootView) {
                return current
            }

            current = current.parent as? View
        }

        return null
    }

    override fun matchesSafely(item: View, mismatchDescription: Description): Boolean {
        val viewNode = ViewUtils.findViewNode(item)
        if (viewNode == null) {
            mismatchDescription.appendText("View does not have a Composer View Node")
            return false
        }

        val rootView = findRootView(item)
        if (rootView == null) {
            mismatchDescription.appendText("View is not a descendant of a ValdiRootView")
            return false
        }

        val itemWithRootView = ValdiElementWithRootView(rootView, viewNode)

        if (innerMatcher.matches(itemWithRootView)) {
            return true
        }

        innerMatcher.describeMismatch(itemWithRootView, mismatchDescription)

        return false
    }
}
