package com.snap.valdi.test

import android.content.Context
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.FrameLayout
import androidx.test.core.app.ApplicationProvider
import androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom
import com.snap.valdi.views.ValdiEditText
import com.snap.valdi.views.ValdiEditTextInput
import com.snap.valdi.views.ValdiEditTextMultiline
import com.snap.valdi.views.ValdiTextViewBase
import org.hamcrest.BaseMatcher
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * Guards the contract behind [ValdiViewMatchers.withTextInputIn].
 *
 * A Valdi textfield is not itself an `EditText`. [ValdiTextViewBase] is a `ViewGroup` whose child
 * [ValdiEditTextInput] is the real `EditText`, so `onView(withAccessibilityId(...))` resolves to
 * the outer group. Espresso's text actions gate on that view: `typeText` requires
 * `onCreateInputConnection() != null`, `replaceText` and `clearText` require
 * `isAssignableFrom(EditText)`. The outer group satisfies neither, so every text action against a
 * bare element matcher fails its constraints — 16 Autopilot tickets' worth.
 *
 * Two halves, because neither is sufficient alone:
 *
 * 1. Behavioral: the matcher picks the inner `EditText` out of a wrapping group and rejects the
 *    group itself. Uses a plain `FrameLayout`/`EditText` stand-in, because this target has no app
 *    resources (`manifest = Config.NONE`) and the `AppCompatEditText`-backed `ValdiEditTextInput`
 *    therefore cannot be constructed here — same constraint `ValdiEditTextSingleLineTest`
 *    documents. Runtime behavior against the real views is exercised on device.
 *
 * 2. Structural: the production classes really do have the wrapped shape the stand-in mimics. This
 *    is the half that would have caught PR #107 — it changed `ValdiEditText` from an `EditText`
 *    into a `ViewGroup`, invalidating every text-action call site, with nothing to flag it.
 *
 * Note what this cannot prove: that each page object actually uses the helper. That is a property
 * of ~12 call sites, not of a unit, and it is covered by the ticketed device tests.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiTextInputMatcherTest {

    private lateinit var outerGroup: FrameLayout
    private lateinit var innerInput: EditText
    private lateinit var unrelatedInput: EditText

    @Before
    fun setUp() {
        val context = ApplicationProvider.getApplicationContext<Context>()

        innerInput = EditText(context)
        outerGroup = FrameLayout(context).apply { addView(innerInput) }
        unrelatedInput = EditText(context)
    }

    /** Stands in for `withAccessibilityId(...)`, which resolves to the element's own view. */
    private fun matchesOuterGroup(): Matcher<View> = object : BaseMatcher<View>() {
        override fun matches(item: Any?) = item === outerGroup
        override fun describeTo(description: Description) {
            description.appendText("the element's own view")
        }
    }

    @Test
    fun resolvesToTheInnerEditText() {
        assertTrue(
            "withTextInputIn must match the EditText inside the matched element",
            ValdiViewMatchers.withTextInputIn(matchesOuterGroup()).matches(innerInput),
        )
    }

    @Test
    fun rejectsTheOuterElementView() {
        assertFalse(
            "withTextInputIn must not match the element's own ViewGroup — that is the view " +
                "Espresso text actions reject",
            ValdiViewMatchers.withTextInputIn(matchesOuterGroup()).matches(outerGroup),
        )
    }

    @Test
    fun rejectsAnEditTextOutsideTheMatchedElement() {
        assertFalse(
            "withTextInputIn must be scoped to descendants of the matched element, otherwise it " +
                "would target any EditText on screen",
            ValdiViewMatchers.withTextInputIn(matchesOuterGroup()).matches(unrelatedInput),
        )
    }

    /**
     * The constraint that actually failed in the 16 tickets: the element's own view is not an
     * `EditText`, so `replaceText` and `clearText` refuse to run against it.
     */
    @Test
    fun theElementViewFailsEspressoTextActionConstraintButTheInnerInputPasses() {
        val editTextConstraint = isAssignableFrom(EditText::class.java)

        assertFalse(
            "the wrapping element view must not satisfy the EditText constraint",
            editTextConstraint.matches(outerGroup),
        )
        assertTrue(
            "the inner input must satisfy the EditText constraint",
            editTextConstraint.matches(innerInput),
        )
    }

    @Test
    fun valdiEditTextIsAWrappingViewGroupAndNotAnEditText() {
        assertTrue(
            "ValdiEditText must be a ViewGroup for withTextInputIn's isDescendantOfA to apply",
            ViewGroup::class.java.isAssignableFrom(ValdiEditText::class.java),
        )
        assertFalse(
            "ValdiEditText must not be an EditText; if it becomes one again, withTextInputIn's " +
                "isDescendantOfA stops matching and every text-action call site must change back",
            EditText::class.java.isAssignableFrom(ValdiEditText::class.java),
        )
        assertTrue(
            "ValdiEditText must remain a ValdiTextViewBase",
            ValdiTextViewBase::class.java.isAssignableFrom(ValdiEditText::class.java),
        )
    }

    @Test
    fun valdiEditTextInputIsTheRealEditText() {
        assertTrue(
            "ValdiEditTextInput is the view text actions must reach, so it must be an EditText",
            EditText::class.java.isAssignableFrom(ValdiEditTextInput::class.java),
        )
    }

    @Test
    fun valdiEditTextMultilineSharesTheWrappedShape() {
        assertTrue(
            "ValdiEditTextMultiline inherits the wrapped shape, so it needs the same matcher " +
                "treatment — it accounted for 7 of the 16 ticketed failures",
            ValdiEditText::class.java.isAssignableFrom(ValdiEditTextMultiline::class.java),
        )
    }
}
