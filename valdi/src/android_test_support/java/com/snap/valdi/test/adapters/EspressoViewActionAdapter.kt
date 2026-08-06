package com.snap.valdi.test.adapters

import android.view.View
import androidx.test.espresso.PerformException
import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import androidx.test.espresso.util.HumanReadables
import com.snap.valdi.test.ElementAction
import com.snap.valdi.test.matchers.ValdiRootViewMatcher
import com.snap.valdi.test.utils.CapturedValdiElementWithRootView
import org.hamcrest.Matcher
import org.hamcrest.StringDescription

/**
 * Espresso ViewAction implementation that performs a ViewNodeAction
 * on the node that matches the given matcher.
 */
class EspressoViewActionAdapter(
    private val capturedElement: CapturedValdiElementWithRootView,
    private val action: ElementAction
) : ViewAction {

    override fun getConstraints(): Matcher<View> {
        return ValdiRootViewMatcher()
    }

    override fun getDescription(): String {
        val description = StringDescription()
        description.appendText("Perform action ")
        description.appendText(action.getDescription())

        return description.toString()
    }

    override fun perform(uiController: UiController, view: View) {
        val element = capturedElement.get(view)
        if (element == null) {
            throw PerformException.Builder()
                .withActionDescription(description)
                .withViewDescription("Composer Element inside ${HumanReadables.describe(view)}")
                .withCause(RuntimeException("Could not find matching element"))
                .build()
        }

        action.perform(uiController, element)
    }
}
