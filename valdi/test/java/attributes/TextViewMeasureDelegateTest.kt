package com.snap.valdi.attributes

import android.content.Context
import android.graphics.Typeface
import android.view.View
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.attributes.impl.richtext.AttributedText
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.ImageAttachmentInfo
import com.snap.valdi.attributes.impl.richtext.InlineViewAttachmentInfo
import com.snap.valdi.attributes.impl.richtext.TextAlignment
import com.snap.valdi.attributes.impl.richtext.TextAnimationTransform
import com.snap.valdi.attributes.impl.richtext.TextDecoration
import com.snap.valdi.attributes.impl.richtext.TextViewMeasureDelegate
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.TextViewUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import kotlin.math.abs

/**
 * Characterization tests for [TextViewMeasureDelegate], the COF-gated direct-measurement path.
 *
 * The delegate exists to replace placeholder-TextView measurement, so the load-bearing property
 * is *parity*: for the same text and font attributes it must agree with what a real TextView
 * measures, or nodes will be sized differently depending on whether the COF flag is on. These
 * tests pin that parity plus the spec-resolution rules, so the measurement behavior survives
 * refactors of the text stack — such as moving text conversion off RichTextConverter and onto
 * [com.snap.valdi.attributes.impl.richtext.ValdiProcessedText.parse].
 */
// NATIVE graphics is required, not incidental: under Robolectric's default legacy graphics,
// Paint/StaticLayout return stubbed metrics that do not vary with font size or line count, so
// every size-sensitivity assertion below passes vacuously (measured 14px for a 14-character
// string at both 12sp and 32sp). Native graphics gives real Skia text metrics.
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
internal class TextViewMeasureDelegateTest {

    private object NoopLogger : Logger {
        override fun log(level: Int, message: String?) = Unit
        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    private fun createFontManager(context: Context): FontManager {
        return FontManager(context, object : TypefaceResLoader {
            override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
        })
    }

    private fun createDelegate(context: Context): TextViewMeasureDelegate {
        return TextViewMeasureDelegate(createFontManager(context), FontAttributes.default, NoopLogger)
    }

    /** Minimal [ViewLayoutAttributes]; the delegate only reads `value` and `fontAttributes`. */
    private class FakeLayoutAttributes(
        private val values: Map<String, Any?>
    ) : ViewLayoutAttributes {
        override fun getValue(attributeName: String): Any? = values[attributeName]
        override fun getBoolValue(attributeName: String): Boolean = values[attributeName] as? Boolean ?: false
        override fun getStringValue(attributeName: String): String? = values[attributeName] as? String
        override fun getDoubleValue(attributeName: String): Double = values[attributeName] as? Double ?: 0.0
    }

    private class SinglePartAttributedText(private val content: String) : AttributedText {
        override fun getPartsSize(): Int = 1
        override fun getContentAtIndex(index: Int): String = content
        override fun getFontAtIndex(index: Int): String? = null
        override fun getTextDecorationAtIndex(index: Int): TextDecoration? = null
        override fun getColorAtIndex(index: Int): Int? = null
        override fun getBackgroundColorAtIndex(index: Int): Int? = null
        override fun getOnTapAtIndex(index: Int): ValdiFunction? = null
        override fun getOnLayoutAtIndex(index: Int): ValdiFunction? = null
        override fun getOutlineColorAtIndex(index: Int): Int? = null
        override fun getOutlineWidthAtIndex(index: Int): Float = 0f
        override fun hasOutline(): Boolean = false
        override fun getAnimationTransformsSize(): Int = 0
        override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? = null
        override fun getInlineViewAttachmentAtIndex(index: Int): InlineViewAttachmentInfo? = null
        override fun getAnimationTransformAtIndex(index: Int): TextAnimationTransform? = null
    }

    private fun unspecified() = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)

    private fun atMost(size: Int) = View.MeasureSpec.makeMeasureSpec(size, View.MeasureSpec.AT_MOST)

    private fun exactly(size: Int) = View.MeasureSpec.makeMeasureSpec(size, View.MeasureSpec.EXACTLY)

    private fun measureWithDelegate(
        context: Context,
        value: Any?,
        fontAttributes: FontAttributes = FontAttributes.default,
        widthSpec: Int = unspecified(),
        heightSpec: Int = unspecified(),
    ): MeasuredSize {
        return createDelegate(context).onMeasure(
            FakeLayoutAttributes(mapOf("value" to value, "fontAttributes" to fontAttributes)),
            widthSpec,
            heightSpec,
            false,
        )
    }

    /**
     * Measures the same string through a real TextView, the way the placeholder measure delegate
     * does, so the two strategies can be compared.
     */
    private fun measureWithPlaceholderTextView(
        context: Context,
        text: String,
        fontAttributes: FontAttributes = FontAttributes.default,
        widthSpec: Int = unspecified(),
        heightSpec: Int = unspecified(),
    ): Pair<Int, Int> {
        val textView = TextView(context)
        TextViewUtils.configure(textView)
        textView.includeFontPadding = false
        textView.typeface = Typeface.DEFAULT
        textView.textSize = fontAttributes.resolvedFontSizeValue
        textView.text = text
        textView.measure(widthSpec, TextViewUtils.resolveHeightMeasureSpec(textView, heightSpec))
        return textView.measuredWidth to textView.measuredHeight
    }

    @Test
    fun measuresPlainStringWithinOnePixelOfAPlaceholderTextView() {
        val context = getApplicationContext<Context>()
        val text = "Measure me"

        val direct = measureWithDelegate(context, text)
        val (placeholderWidth, placeholderHeight) = measureWithPlaceholderTextView(context, text)

        // Tolerance of 1px absorbs the ceil() the delegate applies to the desired width; a real
        // divergence in font metrics or line spacing moves these by far more.
        assertTrue(
            "width: direct=${direct.width()} placeholder=$placeholderWidth",
            abs(direct.width() - placeholderWidth) <= 1
        )
        assertTrue(
            "height: direct=${direct.height()} placeholder=$placeholderHeight",
            abs(direct.height() - placeholderHeight) <= 1
        )
    }

    @Test
    fun measuresAttributedTextTheSameAsTheEquivalentPlainString() {
        val context = getApplicationContext<Context>()
        val text = "Attributed and plain agree"

        val fromString = measureWithDelegate(context, text)
        val fromAttributed = measureWithDelegate(context, SinglePartAttributedText(text))

        // The plain-String branch returns early; the AttributedText branch runs the full
        // ValdiProcessedText.parse conversion. An unstyled single part must land in the same box,
        // which is what catches a broken conversion wiring.
        assertEquals(fromString.width(), fromAttributed.width())
        assertEquals(fromString.height(), fromAttributed.height())
    }

    @Test
    fun attributedTextMeasuresNonZeroWidth() {
        val context = getApplicationContext<Context>()

        val measured = measureWithDelegate(context, SinglePartAttributedText("not empty"))

        // Guards the failure mode where a bad conversion yields an empty CharSequence: the empty
        // branch would return 0x0 and every other assertion here could still pass vacuously.
        assertTrue("width was ${measured.width()}", measured.width() > 0)
        assertTrue("height was ${measured.height()}", measured.height() > 0)
    }

    @Test
    fun emptyTextMeasuresZeroHeight() {
        val context = getApplicationContext<Context>()

        val measured = measureWithDelegate(context, "")

        // iOS-like behavior documented on TextViewUtils.resolveEmptyTextHeightMeasureSpec.
        assertEquals(0, measured.width())
        assertEquals(0, measured.height())
    }

    @Test
    fun unsupportedValueTypeMeasuresEmpty() {
        val context = getApplicationContext<Context>()

        val measured = measureWithDelegate(context, 42)

        assertEquals(0, measured.width())
        assertEquals(0, measured.height())
    }

    @Test
    fun exactlySpecsAreReturnedVerbatim() {
        val context = getApplicationContext<Context>()

        val measured = measureWithDelegate(
            context,
            "text that would otherwise be a different size",
            widthSpec = exactly(120),
            heightSpec = exactly(40),
        )

        assertEquals(120, measured.width())
        assertEquals(40, measured.height())
    }

    @Test
    fun atMostSpecClampsWidth() {
        val context = getApplicationContext<Context>()
        val text = "a string long enough to exceed the bound it is given"

        val unbounded = measureWithDelegate(context, text)
        val bounded = measureWithDelegate(context, text, widthSpec = atMost(50))

        assertTrue("unbounded width was ${unbounded.width()}", unbounded.width() > 50)
        assertEquals(50, bounded.width())
    }

    @Test
    fun multipleAllowedLinesMeasureTallerThanASingleLine() {
        val context = getApplicationContext<Context>()
        val text = "wrap this text across a couple of lines please"
        val width = atMost(80)

        val singleLine = measureWithDelegate(
            context, text, FontAttributes.default.copy(numberOfLines = 1), width
        )
        val threeLines = measureWithDelegate(
            context, text, FontAttributes.default.copy(numberOfLines = 3), width
        )

        assertTrue(
            "single=${singleLine.height()} three=${threeLines.height()}",
            threeLines.height() > singleLine.height()
        )
    }

    @Test
    fun unlimitedLinesMeasureAtLeastAsTallAsThreeLines() {
        val context = getApplicationContext<Context>()
        val text = "wrap this text across a good number of lines so the cap matters"
        val width = atMost(80)

        val threeLines = measureWithDelegate(
            context, text, FontAttributes.default.copy(numberOfLines = 3), width
        )
        // numberOfLines <= 0 means "no cap" per resolveMaxLines.
        val unlimited = measureWithDelegate(
            context, text, FontAttributes.default.copy(numberOfLines = 0), width
        )

        assertTrue(
            "three=${threeLines.height()} unlimited=${unlimited.height()}",
            unlimited.height() >= threeLines.height()
        )
    }

    @Test
    fun largerFontSizeMeasuresLarger() {
        val context = getApplicationContext<Context>()
        val text = "size sensitive"

        val small = measureWithDelegate(context, text, FontAttributes.default.copy(fontSize = 12.0f))
        val large = measureWithDelegate(context, text, FontAttributes.default.copy(fontSize = 32.0f))

        assertTrue("small=${small.width()} large=${large.width()}", large.width() > small.width())
        assertTrue("small=${small.height()} large=${large.height()}", large.height() > small.height())
    }

    @Test
    fun alignmentDoesNotChangeMeasuredSize() {
        val context = getApplicationContext<Context>()
        val text = "alignment is a paint-time concern"

        val normal = measureWithDelegate(context, text, FontAttributes.default.copy(alignment = TextAlignment.LEFT))
        val centered = measureWithDelegate(context, text, FontAttributes.default.copy(alignment = TextAlignment.CENTER))

        assertEquals(normal.width(), centered.width())
        assertEquals(normal.height(), centered.height())
    }

    @Test
    fun lineHeightIncreasesMeasuredHeight() {
        val context = getApplicationContext<Context>()
        val text = "taller lines"

        val normal = measureWithDelegate(context, text)
        val tall = measureWithDelegate(context, text, FontAttributes.default.copy(lineHeight = 2f))

        assertTrue("normal=${normal.height()} tall=${tall.height()}", tall.height() > normal.height())
    }
}
