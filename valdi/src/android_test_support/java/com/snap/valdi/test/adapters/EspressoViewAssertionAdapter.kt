package com.snap.valdi.test.adapters

import android.view.View
import androidx.test.espresso.NoMatchingViewException
import androidx.test.espresso.ViewAssertion
import com.snap.valdi.test.ElementAssertion
import com.snap.valdi.test.utils.CapturedValdiElementWithRootView
import com.snap.valdi.test.utils.assertThat
import org.hamcrest.Matchers

/**
 * Espresso ViewAssertion implementation that performs a ViewNodeAssertion
 * on the node that matches the given matcher.
 */
class EspressoViewAssertionAdapter(
    val capturedElement: CapturedValdiElementWithRootView,
    val assertion: ElementAssertion
) : ViewAssertion {

    override fun check(view: View?, exception: NoMatchingViewException?) {
        if (exception != null) {
            throw exception
        }

        assertThat({ "expecting view " }, view, Matchers.notNullValue())

        if (view != null) {
            val element = capturedElement.get(view)
            assertion.check(element)
        }
    }
}
