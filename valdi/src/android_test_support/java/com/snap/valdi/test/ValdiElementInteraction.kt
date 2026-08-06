package com.snap.valdi.test

import androidx.test.espresso.Espresso
import androidx.test.espresso.ViewAction
import androidx.test.espresso.ViewInteraction
import com.snap.valdi.test.adapters.EspressoViewActionAdapter
import com.snap.valdi.test.adapters.EspressoViewAssertionAdapter
import com.snap.valdi.test.adapters.EspressoViewMatcherAdapter
import com.snap.valdi.test.utils.CapturedValdiElementWithRootView
import org.hamcrest.Matcher

class ValdiElementInteraction(private val matcher: Matcher<ValdiElementWithRootView>) {

    private fun onView(
        cb: (interaction: ViewInteraction, capturedElement: CapturedValdiElementWithRootView) -> Unit
    ): ValdiElementInteraction = apply {
        val capturedElement = CapturedValdiElementWithRootView()
        val viewMatcher = EspressoViewMatcherAdapter(capturedElement, matcher)

        cb(Espresso.onView(viewMatcher), capturedElement)
    }

    fun perform(vararg actions: ElementAction): ValdiElementInteraction {
        return onView { interaction, capturedElement ->
            val adaptedActions = Array<ViewAction>(actions.size) {
                EspressoViewActionAdapter(capturedElement, actions[it])
            }
            interaction.perform(*adaptedActions)
        }
    }

    fun check(assertion: ElementAssertion): ValdiElementInteraction {
        return onView { interaction, capturedElement ->
            interaction.check(EspressoViewAssertionAdapter(capturedElement, assertion))
        }
    }
}
