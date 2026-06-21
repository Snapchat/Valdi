package com.snap.valdi.attributes

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.PathEffect
import android.graphics.Typeface
import android.text.SpannableString
import android.text.Spanned
import android.text.TextPaint
import android.text.style.LineHeightSpan
import android.text.style.MetricAffectingSpan
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.attributes.impl.richtext.CustomUnderlineSpan
import com.snap.valdi.attributes.impl.richtext.CustomUnderlineStyle
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.PatternUnderlineSpan
import com.snap.valdi.attributes.impl.richtext.RichTextConverter
import com.snap.valdi.attributes.impl.richtext.TextDecoration
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class CustomUnderlineSpanTest {
    private fun createFontManager(context: Context): FontManager {
        return FontManager(context, object : TypefaceResLoader {
            override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
        })
    }

    @Test
    fun customUnderlineStyleCreatesCustomUnderlineSpan() {
        val attributes = FontAttributes.default.copy(
            textDecoration = TextDecoration.UNDERLINE,
            customUnderlineStyle = CustomUnderlineStyle(1f, 1f, 1f, -2f)
        )
        val spans = mutableListOf<Any>()
        val fontManager = createFontManager(getApplicationContext())
        val missingFontsTracker = object : MissingFontsTracker {
            override fun onFontMissing(fontDescriptor: FontDescriptor) {}
        }

        attributes.enumerateSpans(fontManager, missingFontsTracker) { spans.add(it) }

        assertTrue(spans.any { it is CustomUnderlineSpan })
    }

    @Test
    fun customUnderlineStyleDoesNotEnableNativeUnderlineFlag() {
        val context = getApplicationContext<Context>()
        val view = TextView(context)
        val helper = TextViewHelper(
            view,
            RichTextConverter(createFontManager(context)),
            FontAttributes.default,
            0
        )

        helper.textValue = "custom underline"
        helper.fontAttributes = FontAttributes.default.copy(
            textDecoration = TextDecoration.UNDERLINE,
            customUnderlineStyle = CustomUnderlineStyle(1f, 1f, 1f, -2f)
        )
        helper.onMeasure(0, 0)

        assertFalse(view.paintFlags and Paint.UNDERLINE_TEXT_FLAG != 0)
    }

    @Test
    fun customUnderlineStyleDoesNotAffectLineHeight() {
        val span: Any = CustomUnderlineSpan(CustomUnderlineStyle(1f, 1f, 1f, 3f))

        assertFalse(span is LineHeightSpan)
    }

    @Test
    fun patternUnderlineSpanClampsUnderlineToLineBottom() {
        val span = ClampedUnderlineSpan(underlineY = 18f, strokeWidth = 4f)

        assertEquals(8f, span.resolveVisibleUnderlineYForTest(Paint(), baseline = 5, bottom = 10), 0.01f)
    }

    @Test
    fun nativeUnderlineStillEnablesNativeUnderlineFlag() {
        val context = getApplicationContext<Context>()
        val view = TextView(context)
        val helper = TextViewHelper(
            view,
            RichTextConverter(createFontManager(context)),
            FontAttributes.default,
            0
        )

        helper.textValue = "native underline"
        helper.fontAttributes = FontAttributes.default.copy(textDecoration = TextDecoration.UNDERLINE)
        helper.onMeasure(0, 0)

        assertTrue(view.paintFlags and Paint.UNDERLINE_TEXT_FLAG != 0)
    }

    @Test
    fun patternUnderlineSpanMeasuresMetricAffectingSpans() {
        val text = SpannableString("plain wide")
        val spanStart = "plain ".length
        val underlineSpan = CapturingUnderlineSpan()
        text.setSpan(WideTextSpan, spanStart, text.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        text.setSpan(underlineSpan, spanStart, text.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)

        val paint = TextPaint().apply {
            density = 1f
            textSize = 20f
        }
        val canvas = Canvas(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888))

        underlineSpan.drawBackground(
            canvas,
            paint,
            0,
            1000,
            0,
            20,
            30,
            text,
            0,
            text.length,
            0
        )

        val expectedStartX = paint.measureText(text, 0, spanStart)
        val widePaint = TextPaint(paint).apply { textScaleX *= 2f }
        val expectedEndX = expectedStartX + widePaint.measureText(text, spanStart, text.length)

        assertEquals(expectedStartX, underlineSpan.startX, 0.01f)
        assertEquals(expectedEndX, underlineSpan.endX, 0.01f)
    }

    private object WideTextSpan : MetricAffectingSpan() {
        override fun updateDrawState(textPaint: TextPaint) {
            textPaint.textScaleX *= 2f
        }

        override fun updateMeasureState(textPaint: TextPaint) {
            textPaint.textScaleX *= 2f
        }
    }

    private class CapturingUnderlineSpan : PatternUnderlineSpan() {
        var startX: Float = Float.NaN
        var endX: Float = Float.NaN

        override fun resolveStartX(startX: Float, strokeWidth: Float, density: Float): Float {
            this.startX = startX
            return startX
        }

        override fun resolveEndX(endX: Float, strokeWidth: Float, density: Float): Float {
            this.endX = endX
            return endX
        }

        override fun createPathEffect(strokeWidth: Float, density: Float): PathEffect? = null
    }

    private class ClampedUnderlineSpan(
        private val underlineY: Float,
        private val strokeWidth: Float
    ) : PatternUnderlineSpan() {
        override fun resolveStrokeWidth(paint: Paint, density: Float): Float = strokeWidth

        fun resolveVisibleUnderlineYForTest(paint: Paint, baseline: Int, bottom: Int): Float {
            return resolveVisibleUnderlineY(paint, baseline, bottom, strokeWidth, density = 1f)
        }

        override fun resolveUnderlineY(
            paint: Paint,
            baseline: Int,
            strokeWidth: Float,
            density: Float
        ): Float = underlineY

        override fun createPathEffect(strokeWidth: Float, density: Float): PathEffect? = null
    }
}
