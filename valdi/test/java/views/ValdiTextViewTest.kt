package com.snap.valdi.views

import android.content.Context
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class ValdiTextViewTest {
    @Test
    fun labelSelectionIsDisabledByDefault() {
        val textView = ValdiTextView(getApplicationContext<Context>())
        textView.text = "Selectable label text"

        assertFalse(textView.isTextSelectable)
    }

    @Test
    fun labelSelectableTogglesNativeTextSelection() {
        val textView = ValdiTextView(getApplicationContext<Context>())
        textView.text = "Selectable label text"

        textView.setValdiSelectable(true)
        assertTrue(textView.isTextSelectable)

        textView.setValdiSelectable(false)
        assertFalse(textView.isTextSelectable)
    }
}
