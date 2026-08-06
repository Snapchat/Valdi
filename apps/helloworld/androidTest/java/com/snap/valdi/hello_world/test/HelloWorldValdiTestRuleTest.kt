package com.snap.valdi.hello_world.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.snap.valdi.test.ValdiElementActions
import com.snap.valdi.test.ValdiElementAssertions
import com.snap.valdi.test.ValdiElementMatchers
import com.snap.valdi.test.ValdiEspresso
import com.snap.valdi.test.ValdiTestRule
import com.snap.valdi.views.ValdiRootView
import org.hamcrest.Matchers.startsWith
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Mounts a single Valdi component with [ValdiTestRule] — no app activity involved — and asserts
 * on it with the Valdi Espresso bindings. This is the component-level counterpart to
 * [HelloWorldAppTest] and the reference example for the rule itself.
 */
@RunWith(AndroidJUnit4::class)
class HelloWorldValdiTestRuleTest {

    @get:Rule
    val valdi = ValdiTestRule { runtime ->
        ValdiRootView(runtime.context).also { rootView ->
            rootView.setRetainsLayoutSpecsOnInvalidateLayout(true)
            runtime.inflateViewAsync(rootView, COMPONENT_PATH, null, null, null, null)
        }
    }

    @Test
    fun mountsTheComponent() {
        valdi.waitForNextRender()

        assertNotNull("Expected a view node once the component rendered", valdi.view.valdiViewNode)
    }

    @Test
    fun findsElementByAccessibilityId() {
        valdi.waitForNextRender()

        ValdiEspresso.onValdiElement(ValdiElementMatchers.withAccessibilityId(WELCOME_LABEL_ID))
            .check(
                ValdiElementAssertions.matches(
                    ValdiElementMatchers.withText(startsWith(WELCOME_PREFIX))
                )
            )
    }

    @Test
    fun findsElementByType() {
        valdi.waitForNextRender()

        ValdiEspresso.onValdiElement(ValdiElementMatchers.isScroll())
            .check(
                ValdiElementAssertions.matches(
                    ValdiElementMatchers.hasDescendant(
                        ValdiElementMatchers.withAccessibilityId(WELCOME_LABEL_ID)
                    )
                )
            )
    }

    @Test
    fun performsActionOnElement() {
        valdi.waitForNextRender()

        ValdiEspresso.onValdiElement(ValdiElementMatchers.withAccessibilityId(WELCOME_LABEL_ID))
            .perform(ValdiElementActions.setAttribute("value", REPLACED_TEXT))

        ValdiEspresso.waitUntil(ValdiElementMatchers.withText(REPLACED_TEXT))
    }

    @Test
    fun destroyViewUnmountsTheComponent() {
        valdi.waitForNextRender()
        val view = valdi.view

        valdi.destroyView()

        assertTrue("Expected the root view to be destroyed", view.destroyed)
        assertNull("Expected the root view to be detached from the host activity", view.parent)
    }

    private companion object {
        const val COMPONENT_PATH = "App@hello_world/src/HelloWorldApp"
        const val WELCOME_LABEL_ID = "welcome-label"
        const val WELCOME_PREFIX = "Welcome to "
        const val REPLACED_TEXT = "Set from an instrumented test"
    }
}
