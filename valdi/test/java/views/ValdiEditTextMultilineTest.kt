package com.snap.valdi.views

import android.graphics.Color
import android.text.Spannable
import android.text.SpannableString
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
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
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiEditTextMultilineTest {

    // Case 1: repeated return-type binds (multiline -> done -> done -> multiline) must be idempotent
    // and never crash. The redundant bind is what re-copied the live buffer in the crash reports.
    @Test
    fun repeatedReturnTypeBind_isIdempotent_andDoesNotThrow() {
        val view = ValdiEditTextMultiline(getApplicationContext())
        setAttributedMode(view)
        view.setText(SpannableString("hello"))
        view.setSelectionClamped(5, 5)

        val genAfterSetup = view.setTextGeneration
        // multiline -> done: a real transition refreshes the buffer exactly once.
        view.allowLineReturns(false)
        val genAfterFirst = view.setTextGeneration
        assertTrue("a real return-type transition should refresh the buffer", genAfterFirst > genAfterSetup)

        // done -> done: idempotent no-op, must not refresh again.
        view.allowLineReturns(false)
        assertEquals("a repeated bind must not refresh the buffer again", genAfterFirst, view.setTextGeneration)

        // Toggling back stays safe and preserves the content.
        view.allowLineReturns(true)
        view.allowLineReturns(true)
        assertEquals("hello", view.text.toString())
    }

    // Case 2: attributed text (with spans) present when inputType changes must not crash, and the
    // content plus its spans must survive the transition in both directions.
    @Test
    fun attributedTextWithSpans_survivesReturnTypeTransition() {
        val view = ValdiEditTextMultiline(getApplicationContext())
        val spannable = SpannableString("hello world")
        spannable.setSpan(ForegroundColorSpan(Color.RED), 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        setAttributedMode(view)
        view.setText(spannable)
        view.setSelectionClamped(3, 3)

        view.allowLineReturns(false)
        view.allowLineReturns(true)

        assertEquals("hello world", view.text.toString())
        val text = view.text as Spanned
        assertEquals(1, text.getSpans(0, text.length, ForegroundColorSpan::class.java).size)
        assertEquals(3, view.selectionStart)
        assertEquals(3, view.selectionEnd)
    }

    // Case 3: a stale selection that points past the resulting text length must be clamped to the
    // actual post-setText length, and the processed (truncated) text is what gets set.
    @Test
    fun staleSelectionPastNewLength_isClampedToActualLength() {
        val view = ValdiEditText(getApplicationContext())
        view.setCharacterLimit(2)
        invokeSetSpannableAndSelection(view, SpannableString("abcde"), start = 4, end = 4)

        assertEquals("ab", view.text.toString())
        assertEquals(2, view.selectionStart)
        assertEquals(2, view.selectionEnd)
    }

    // Case 4: a post-setText empty buffer with a nonzero prior selection must clamp to zero rather
    // than apply "setSpan (2 ... 2) ends beyond length 0".
    @Test
    fun emptyBufferWithNonzeroPriorSelection_clampsToZero() {
        val view = ValdiEditText(getApplicationContext())
        view.setCharacterLimit(0)
        invokeSetSpannableAndSelection(view, SpannableString("ab"), start = 2, end = 2)

        assertEquals("", view.text.toString())
        assertEquals(0, view.selectionStart)
        assertEquals(0, view.selectionEnd)
    }

    // Directly exercises the array-index copy family: a buffer whose span array throws while being
    // copied must degrade to span-less text, not propagate the ArrayIndexOutOfBoundsException.
    @Test
    fun corruptSpanArray_duringCopy_degradesGracefully() {
        val view = ValdiEditText(getApplicationContext())
        invokeSetSpannableAndSelection(view, ThrowingSpannable("hi there"), start = 0, end = 2)

        assertEquals("hi there", view.text.toString())
        assertEquals(0, view.selectionStart)
        assertEquals(2, view.selectionEnd)
    }

    private fun setAttributedMode(view: ValdiEditText) {
        ValdiEditText::class.java.getDeclaredField("isAttributedText").apply {
            isAccessible = true
        }.setBoolean(view, true)
    }

    private fun invokeSetSpannableAndSelection(view: ValdiEditText, spannable: Spannable, start: Int, end: Int) {
        ValdiEditText::class.java.getDeclaredMethod(
            "setSpannableAndSelection",
            Spannable::class.java,
            Int::class.javaPrimitiveType!!,
            Int::class.javaPrimitiveType!!,
            Boolean::class.javaPrimitiveType!!,
        ).apply {
            isAccessible = true
        }.invoke(view, spannable, start, end, true)
    }

    /** A Spannable whose span array is "corrupt": copying it (SpannableStringBuilder) throws, matching
     *  the framework's getSpansRec ArrayIndexOutOfBoundsException seen in the crash reports. */
    private class ThrowingSpannable(text: CharSequence) : SpannableString(text) {
        override fun <T : Any?> getSpans(queryStart: Int, queryEnd: Int, kind: Class<T>): Array<T> {
            throw ArrayIndexOutOfBoundsException("length=1; index=1")
        }
    }
}
