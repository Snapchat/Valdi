package com.snap.valdi.attributes.impl.richtext

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Typeface
import android.view.View
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.logger.Logger
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode

/**
 * Guards that re-binding an identical plain String costs nothing, by counting requestLayout() calls
 * rather than timing. Counting is deliberate: the multiplier is exact and deterministic, where wall
 * clock on an emulator is not comparable to the device Perfetto numbers in 09b84f4e8dff.
 *
 * TextViewHelper.isTextValueEqual only short-circuits for String, so this is the path where the
 * setter can prove nothing changed. It is asserted here so it cannot quietly stop short-circuiting.
 *
 * The attributed-text path does not short-circuit in this setter, and for reference the cost when it
 * does redundantly re-bind, per 10 binds of the same instance, measures:
 *   bind only, no measure : 10 requestLayout, 0 re-applications
 *   bind + measure        : 20 requestLayout, 10 re-applications
 * i.e. one invalidation from onDirty() plus one from the TextView.setText inside the re-apply,
 * about half the cost of a legitimate first bind (4).
 *
 * That is NOT a production cost, and this class deliberately does not assert against it: rich text
 * arrives natively as a plain array, so ViewNodeAttribute compares it structurally and skips
 * onApply entirely when it has not changed, meaning the setter is never reached for the
 * unchanged-text case. See AnimationRichTextTest
 * .textValueSetterDoesNotBypassEqualityForStaticAttributedText for the full reasoning.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
internal class RedundantRebindCostTest {

    private lateinit var fontManager: FontManager

    private object NoopLogger : Logger {
        override fun log(level: Int, message: String?) = Unit
        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    /**
     * Counts the layout invalidations onDirty() produces. isLayoutRequested is overridden because
     * onDirty() consults it to dedupe within a pending traversal, and clearLayoutRequest() models
     * the traversal completing between binds.
     */
    private class CountingTextView(context: Context) : TextView(context) {
        var requestLayoutCount = 0
        private var layoutRequested = false

        override fun requestLayout() {
            requestLayoutCount++
            layoutRequested = true
            super.requestLayout()
        }

        override fun isLayoutRequested(): Boolean = layoutRequested

        fun clearLayoutRequest() {
            layoutRequested = false
        }
    }

    @Before
    fun setUp() {
        val appInfo = getApplicationContext<Context>().applicationInfo
        appInfo.targetSdkVersion = 28
        appInfo.flags = appInfo.flags or ApplicationInfo.FLAG_SUPPORTS_RTL
        fontManager = FontManager(getApplicationContext(), object : TypefaceResLoader {
            override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
        })
    }

    @Test
    fun redundantPlainStringBindsCostNothing() {
        val passes = 10
        val view = CountingTextView(getApplicationContext())
        val helper = TextViewHelper(view, fontManager, FontAttributes.default, 0, NoopLogger)

        helper.textValue = "Alex Johnson"
        helper.onMeasure(
            View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.AT_MOST),
            View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
        )
        val layoutsAfterFirst = view.requestLayoutCount

        repeat(passes) {
            view.clearLayoutRequest()
            helper.textValue = "Alex Johnson"
            helper.onMeasure(
                View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.AT_MOST),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
            )
        }

        // The String branch of isTextValueEqual short-circuits correctly, so this stays flat.
        // Guards that path against regressing the same way the attributed path did.
        assertEquals(
            "redundant binds of an identical String must not invalidate layout",
            0,
            view.requestLayoutCount - layoutsAfterFirst,
        )
    }

}
