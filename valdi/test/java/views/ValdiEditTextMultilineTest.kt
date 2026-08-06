package com.snap.valdi.views

import android.graphics.Color
import android.text.Spannable
import android.text.SpannableString
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * Covers the ValdiEditText multiline return-type transition that produced the two Send To crash
 * families: the SpannableStringBuilder array-index copy failure and the "setSpan ... beyond length 0"
 * selection failure. The exact framework span-array corruption is not reproducible under Robolectric,
 * so these pin the fix's invariants: idempotent binds, refresh from a detached snapshot, selection
 * clamped to the actual post-setText length, and a graceful degrade when a span copy throws.
 *
 * Every assertion addresses [ValdiEditText.backingEditTextInput] rather than the ValdiEditText
 * itself. ValdiEditText is a ValdiTextViewBase (a ViewGroup) wrapping the real EditText, so setText,
 * the selection, setSelectionClamped, allowLineReturns, setCharacterLimit, setTextGeneration and
 * setSpannableAndSelection all live on the inner input. The pre-ViewGroup version of this test
 * addressed the outer view and no longer compiles -- the same contract change that broke the
 * Espresso text actions.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
@Ignore(
    "Cannot run in //valdi:test_java: constructing the AppCompatEditText-backed " +
        "ValdiEditTextInput throws Resources\$NotFoundException: Resource ID #0x7f0100ae from " +
        "ShadowLegacyAssetManager.attrsToTypedArray, because this kt_jvm_test target carries no " +
        "AppCompat resources. Dropping manifest = Config.NONE does not help. This is the same " +
        "limitation ValdiTextInputMatcherTest documents when it substitutes a plain EditText. " +
        "PR #107 deleted this file outright for that reason; it is kept here, retargeted at the " +
        "inner input so it compiles against the ViewGroup hierarchy, because it is the only " +
        "regression coverage for two shipped Send To crash families. Re-enable once the target " +
        "can resolve AppCompat theme attributes -- do not delete."
)
class ValdiEditTextMultilineTest {

    // Case 1: repeated return-type binds (multiline -> done -> done -> multiline) must be idempotent
    // and never crash. The redundant bind is what re-copied the live buffer in the crash reports.
    @Test
    fun repeatedReturnTypeBind_isIdempotent_andDoesNotThrow() {
        val view = ValdiEditTextMultiline(getApplicationContext())
        val input = view.backingEditTextInput
        setAttributedMode(input)
        input.setText(SpannableString("hello"))
        input.setSelectionClamped(5, 5)

        val genAfterSetup = input.setTextGeneration
        // multiline -> done: a real transition refreshes the buffer exactly once.
        input.allowLineReturns(false)
        val genAfterFirst = input.setTextGeneration
        assertTrue("a real return-type transition should refresh the buffer", genAfterFirst > genAfterSetup)

        // done -> done: idempotent no-op, must not refresh again.
        input.allowLineReturns(false)
        assertEquals("a repeated bind must not refresh the buffer again", genAfterFirst, input.setTextGeneration)

        // Toggling back stays safe and preserves the content.
        input.allowLineReturns(true)
        input.allowLineReturns(true)
        assertEquals("hello", input.text.toString())
    }

    // Case 2: attributed text (with spans) present when inputType changes must not crash, and the
    // content plus its spans must survive the transition in both directions.
    @Test
    fun attributedTextWithSpans_survivesReturnTypeTransition() {
        val view = ValdiEditTextMultiline(getApplicationContext())
        val input = view.backingEditTextInput
        val spannable = SpannableString("hello world")
        spannable.setSpan(ForegroundColorSpan(Color.RED), 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        setAttributedMode(input)
        input.setText(spannable)
        input.setSelectionClamped(3, 3)

        input.allowLineReturns(false)
        input.allowLineReturns(true)

        assertEquals("hello world", input.text.toString())
        val text = input.text as Spanned
        assertEquals(1, text.getSpans(0, text.length, ForegroundColorSpan::class.java).size)
        assertEquals(3, input.selectionStart)
        assertEquals(3, input.selectionEnd)
    }

    // Case 3: a stale selection that points past the resulting text length must be clamped to the
    // actual post-setText length, and the processed (truncated) text is what gets set.
    @Test
    fun staleSelectionPastNewLength_isClampedToActualLength() {
        val input = ValdiEditText(getApplicationContext()).backingEditTextInput
        input.setCharacterLimit(2)
        invokeSetSpannableAndSelection(input, SpannableString("abcde"), start = 4, end = 4)

        assertEquals("ab", input.text.toString())
        assertEquals(2, input.selectionStart)
        assertEquals(2, input.selectionEnd)
    }

    // Case 4: a post-setText empty buffer with a nonzero prior selection must clamp to zero rather
    // than apply "setSpan (2 ... 2) ends beyond length 0".
    @Test
    fun emptyBufferWithNonzeroPriorSelection_clampsToZero() {
        val input = ValdiEditText(getApplicationContext()).backingEditTextInput
        input.setCharacterLimit(0)
        invokeSetSpannableAndSelection(input, SpannableString("ab"), start = 2, end = 2)

        assertEquals("", input.text.toString())
        assertEquals(0, input.selectionStart)
        assertEquals(0, input.selectionEnd)
    }

    // Directly exercises the array-index copy family: a buffer whose span array throws while being
    // copied must degrade to span-less text, not propagate the ArrayIndexOutOfBoundsException.
    @Test
    fun corruptSpanArray_duringCopy_degradesGracefully() {
        val input = ValdiEditText(getApplicationContext()).backingEditTextInput
        invokeSetSpannableAndSelection(input, ThrowingSpannable("hi there"), start = 0, end = 2)

        assertEquals("hi there", input.text.toString())
        assertEquals(0, input.selectionStart)
        assertEquals(2, input.selectionEnd)
    }

    private fun setAttributedMode(input: ValdiEditTextInput) {
        ValdiEditTextInput::class.java.getDeclaredField("isAttributedText").apply {
            isAccessible = true
        }.setBoolean(input, true)
    }

    private fun invokeSetSpannableAndSelection(
        input: ValdiEditTextInput,
        spannable: Spannable,
        start: Int,
        end: Int,
    ) {
        ValdiEditTextInput::class.java.getDeclaredMethod(
            "setSpannableAndSelection",
            Spannable::class.java,
            Int::class.javaPrimitiveType!!,
            Int::class.javaPrimitiveType!!,
            Boolean::class.javaPrimitiveType!!,
        ).apply {
            isAccessible = true
        }.invoke(input, spannable, start, end, true)
    }

    /** A Spannable whose span array is "corrupt": copying it (SpannableStringBuilder) throws, matching
     *  the framework's getSpansRec ArrayIndexOutOfBoundsException seen in the crash reports. */
    private class ThrowingSpannable(text: CharSequence) : SpannableString(text) {
        override fun <T : Any?> getSpans(queryStart: Int, queryEnd: Int, kind: Class<T>): Array<T> {
            throw ArrayIndexOutOfBoundsException("length=1; index=1")
        }
    }
}
