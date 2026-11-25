package com.snap.valdi.hello_world.test

import android.content.Context
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.TextView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewAssertion
import androidx.test.espresso.action.ViewActions.clearText
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.closeSoftKeyboard
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasFocus
import androidx.test.espresso.matcher.ViewMatchers.withHint
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.helloworld.textbinding.HelloWorldEmojiTextView
import com.snap.valdi.helloworld.textbinding.HelloWorldTextView
import com.snap.valdi.helloworld.textbinding.TextBindingFixtureBinders
import com.snap.valdi.test.ValdiTestRule
import com.snap.valdi.test.ValdiViewMatchers
import com.snap.valdi.views.ValdiEditText
import com.snap.valdi.views.ValdiEditTextInput
import com.snap.valdi.views.ValdiRecyclableView
import com.snap.valdi.views.ValdiRootView
import com.snap.valdi.views.ValdiTextBindableView
import com.snap.valdi.views.ValdiTextHolder
import com.snap.valdi.views.ValdiTextViewBase
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Contract for the Android text views, checked on a real runtime.
 *
 * Mounts the fixture component (`TextContractFixture.tsx`) with the host-app text views from
 * `//apps/helloworld/text_binding_fixture` registered the way an app registers its own, and checks
 * three things app code and Espresso tests depend on:
 *
 * - the view class hierarchy, which Espresso's text actions gate on;
 * - focus and text actions reaching the platform EditText;
 * - Valdi text attributes reaching each text view through its binder.
 *
 * The hierarchy tests are the guard for view-type changes: turning the platform text views into
 * containers, or dropping [ValdiTextBindableView] from a host view, fails here rather than in app
 * tests.
 */
@Suppress("DEPRECATION")
@RunWith(AndroidJUnit4::class)
class TextViewContractTest {

    @get:Rule
    val valdi: ValdiTestRule<ValdiRootView> = ValdiTestRule { runtime ->
        TextBindingFixtureBinders.register(
            runtime,
            runtime.context,
            valdi.runtimeManager.fontManager,
            FontAttributes.default,
            valdi.runtimeManager.logger,
        )
        ValdiRootView(runtime.context).also { rootView ->
            rootView.setRetainsLayoutSpecsOnInvalidateLayout(true)
            runtime.inflateViewAsync(rootView, COMPONENT_PATH, null, null, null, null)
        }
    }

    private val context: Context
        get() = getInstrumentation().targetContext

    private fun render() = valdi.waitForNextRender()

    private fun textInput() = ValdiViewMatchers.withTextInputAccessibilityId(INPUT_ID)

    private fun <T> onMain(block: () -> T): T {
        var result: T? = null
        getInstrumentation().runOnMainSync { result = block() }
        @Suppress("UNCHECKED_CAST")
        return result as T
    }

    private fun viewAssertion(check: (View) -> Unit) = ViewAssertion { view, noMatch ->
        if (view == null) throw noMatch ?: AssertionError("No view matched")
        check(view)
    }

    @Test
    fun textInputKeepsThePlatformEditTextShape() {
        val input: View = onMain { ValdiEditTextInput(context) }

        assertTrue("the input must stay an EditText", input is EditText)
        assertFalse("the input must not become a container", input is ViewGroup)
    }

    @Test
    fun textfieldWrapsExactlyOneEditText() {
        val field = onMain { ValdiEditText(context) }
        val asView: View = field

        assertTrue(asView is ValdiTextViewBase)
        assertTrue(asView is ValdiTextBindableView)
        assertEquals(1, field.childCount)
        assertSame(field.backingEditTextInput, field.getChildAt(0))
        assertSame(field.backingEditTextInput, field.bindingTextView)
    }

    @Test
    fun emojiTextViewStaysAPlainTextViewThatBindsText() {
        val view: View = onMain { HelloWorldEmojiTextView(context) }

        assertTrue("must be a TextView", view is TextView)
        assertFalse("must not become a container", view is ViewGroup)
        assertTrue("must implement ValdiTextBindableView", view is ValdiTextBindableView)
        assertTrue("must implement ValdiTextHolder", view is ValdiTextHolder)
        assertTrue("must implement ValdiRecyclableView", view is ValdiRecyclableView)
        assertSame(view, (view as ValdiTextBindableView).bindingTextView)
    }

    @Test
    fun hostLabelIsAValdiTextViewBase() {
        val view: View = onMain { HelloWorldTextView(context) }

        assertTrue(view is ValdiTextViewBase)
        assertTrue(view is ValdiTextBindableView)
        val backing: View = (view as ValdiTextBindableView).bindingTextView
        assertFalse(backing is ViewGroup)
    }

    @Test
    fun textfieldResolvesToTheEditTextThroughTheMatcher() {
        render()

        onView(textInput()).check(viewAssertion { view ->
            assertTrue("matched ${view.javaClass.name}", view is EditText)
            assertTrue("parent is ${view.parent?.javaClass?.name}", view.parent is ValdiEditText)
        })
    }

    @Test
    fun textfieldReceivesBoundTextAndPlaceholder() {
        render()

        onView(textInput()).check(matches(withText("Initial")))
        onView(textInput()).check(matches(withHint("Type here")))
    }

    @Test
    fun clickFocusesTheEditText() {
        render()

        onView(textInput()).perform(click())

        onView(textInput()).check(matches(hasFocus()))
    }

    @Test
    fun replaceTextAndClearTextReachTheEditText() {
        render()

        onView(textInput()).perform(replaceText("Replaced"))
        onView(textInput()).check(matches(withText("Replaced")))

        onView(textInput()).perform(clearText())
        onView(textInput()).check(matches(withText("")))
    }

    @Test
    fun typeTextReachesTheEditText() {
        render()

        onView(textInput()).perform(click(), clearText(), typeText("typed"), closeSoftKeyboard())

        onView(textInput()).check(matches(withText("typed")))
    }

    @Test
    fun labelReceivesBoundText() {
        render()

        onView(ValdiViewMatchers.withAccessibilityId("contract-label")).check(viewAssertion { view ->
            assertTrue(view is ValdiTextViewBase)
            assertEquals("Label text", (view as ValdiTextViewBase).bindingTextView.text.toString())
        })
    }

    @Test
    fun emojiTextViewReceivesBoundText() {
        render()

        onView(ValdiViewMatchers.withAccessibilityId("contract-emoji")).check(viewAssertion { view ->
            assertTrue("matched ${view.javaClass.name}", view is HelloWorldEmojiTextView)
        })
        onView(ValdiViewMatchers.withAccessibilityId("contract-emoji")).check(matches(withText("Hello 👋")))
    }

    @Test
    fun hostLabelReceivesBoundTextAndCustomAttribute() {
        render()

        onView(ValdiViewMatchers.withAccessibilityId("contract-host-label")).check(viewAssertion { view ->
            assertTrue("matched ${view.javaClass.name}", view is HelloWorldTextView)
            val label = view as HelloWorldTextView
            assertEquals("Host label", label.text.toString())
            assertEquals("Host placeholder", label.placeholder.toString())
        })
    }

    private companion object {
        const val COMPONENT_PATH = "TextContractFixture@hello_world/src/TextContractFixture"
        const val INPUT_ID = "contract-input"
    }
}
