package com.snap.valdi.test

import android.os.Bundle
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.appcompat.app.AppCompatActivity

/**
 * Bare activity that hosts a Valdi root view for the duration of an instrumented test.
 *
 * [ValdiTestRule] launches it; tests never reference it directly. It is declared in this
 * library's manifest, so depending on `//valdi:valdi_android_test_runtime` is enough — no
 * entry is needed in the consuming project's androidTest manifest.
 */
class ValdiTestHostActivity : AppCompatActivity() {

    /** Container that [ValdiTestRule] adds the view under test to. */
    lateinit var contentContainer: ViewGroup
        private set

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val container = FrameLayout(this)
        contentContainer = container
        setContentView(container)
    }
}
