package com.snap.valdi.views

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * Structural guards for single-line clamping on [ValdiEditTextInput].
 *
 * Android's `TextView.setInputType` runs `applySingleLine(false)` as a side-effect for
 * multi-line-ish input types, resetting `maxLines` to `Integer.MAX_VALUE` and turning off
 * horizontal scrolling — silently letting a single-line field wrap. The fix is the
 * `setMaxLines` / `setHorizontallyScrolling` overrides plus the mutable `isValdiSingleLine`
 * flag in ValdiEditText.kt (which [ValdiEditTextMultiline] flips off).
 *
 * This fix lives only on the Snap fork and was dropped once already during the PR #107 rebase,
 * so these tests fail loudly if the override or flag is removed again. They assert structure via
 * reflection rather than runtime behavior because this test target has no app resources
 * (`manifest = Config.NONE`), so an AppCompatEditText-backed view can't be constructed here;
 * runtime clamping is exercised by the device-level composer-sample-instrumentation tests.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class ValdiEditTextSingleLineTest {

    @Test
    fun valdiEditTextInputDeclaresMutableSingleLineFlag() {
        val field = ValdiEditTextInput::class.java.getDeclaredField("isValdiSingleLine")
        assertEquals(Boolean::class.javaPrimitiveType, field.type)
    }

    @Test
    fun valdiEditTextInputOverridesSetMaxLinesForClamping() {
        val method = ValdiEditTextInput::class.java.getDeclaredMethod(
            "setMaxLines",
            Int::class.javaPrimitiveType,
        )
        assertNotNull(method)
    }

    @Test
    fun valdiEditTextInputOverridesSetHorizontallyScrollingForClamping() {
        val method = ValdiEditTextInput::class.java.getDeclaredMethod(
            "setHorizontallyScrolling",
            Boolean::class.javaPrimitiveType,
        )
        assertNotNull(method)
    }
}
