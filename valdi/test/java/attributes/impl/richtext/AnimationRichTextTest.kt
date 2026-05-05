package com.snap.valdi.attributes.impl.richtext

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.PointF
import android.graphics.Typeface
import android.text.TextPaint
import android.text.Layout
import android.text.StaticLayout
import android.view.Gravity
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
import com.snap.valdi.attributes.impl.richtext.InvisibleForegroundColorSpan
import com.snap.valdi.attributes.impl.richtext.InvisibleReplacementSpan
import com.snap.valdi.attributes.impl.richtext.RichTextConverter
import com.snap.valdi.attributes.impl.richtext.TextAlignment
import com.snap.valdi.attributes.impl.richtext.TextAnimationTransform
import com.snap.valdi.attributes.impl.richtext.TextDecoration
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.attributes.impl.richtext.hasActiveAnimationTransform
import com.snap.valdi.attributes.impl.richtext.hasRenderableAnimationTransform
import com.snap.valdi.attributes.impl.richtext.isActiveAnimationTransform
import com.snap.valdi.attributes.impl.richtext.isRenderableAnimationTransform
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.views.TextViewUtils
import com.snap.valdi.views.ValdiTextView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.Shadows.shadowOf

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class AnimationRichTextTest {
    private lateinit var fontManager: FontManager
    private lateinit var converter: RichTextConverter
    private lateinit var missingFontsTracker: MissingFontsTracker

    @Before
    fun setUp() {
        val appInfo = getApplicationContext<Context>().applicationInfo
        appInfo.targetSdkVersion = 28
        appInfo.flags = appInfo.flags or ApplicationInfo.FLAG_SUPPORTS_RTL

        fontManager = FontManager(
            getApplicationContext(),
            object : TypefaceResLoader {
                override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
            },
        )
        converter = RichTextConverter(fontManager)
        missingFontsTracker = object : MissingFontsTracker {
            override fun onFontMissing(fontDescriptor: FontDescriptor) = Unit
        }
    }

    @Test
    fun activeAnimationCanBeNonRenderable() {
        val transform = TextAnimationTransform(0f, 1f, 0f)
        assertTrue(isActiveAnimationTransform(transform))
        assertFalse(isRenderableAnimationTransform(transform))
    }

    @Test
    fun attributedTextReportsActiveAnimation() {
        val text = FakeAttributedText(
            listOf(
                Part("a", null),
                Part("b", TextAnimationTransform(0f, 1f, 0f)),
            )
        )

        assertTrue(text.hasActiveAnimationTransform())
        assertFalse(text.hasRenderableAnimationTransform())
    }

    @Test
    fun invisibleReplacementSpanHandlesNullText() {
        val span = InvisibleReplacementSpan()
        assertEquals(0, span.getSize(Paint(), null, 0, 0, null))
    }

    @Test
    fun invisibleReplacementSpanRoundsWidthUp() {
        val paint = Paint().apply { textSize = 17f }
        val text = "Hello"
        val span = InvisibleReplacementSpan()

        assertEquals(
            kotlin.math.ceil(paint.measureText(text).toDouble()).toInt(),
            span.getSize(paint, text, 0, text.length, null),
        )
    }

    @Test
    fun overlayRenderModeKeepsStaticFillVisible() {
        val spans = mutableListOf<Any>()
        FontAttributes.default.copy(
            color = Color.RED,
            alignment = TextAlignment.LEFT,
            animationTransform = null,
        ).enumerateSpans(
            fontManager = fontManager,
            missingFontsTracker = missingFontsTracker,
            renderMode = FontAttributes.RenderMode.OVERLAY,
            closure = { spans.add(it) },
        )

        assertTrue(spans.any { it is android.text.style.ForegroundColorSpan })
        assertFalse(spans.any { it is InvisibleReplacementSpan })
    }

    @Test
    fun overlayRenderModeKeepsInactiveAnimatedFillVisibleInCache() {
        val spans = mutableListOf<Any>()
        FontAttributes.default.copy(
            color = Color.RED,
            alignment = TextAlignment.LEFT,
            animationTransform = TextAnimationTransform(0f, 1f, 1f),
        ).enumerateSpans(
            fontManager = fontManager,
            missingFontsTracker = missingFontsTracker,
            renderMode = FontAttributes.RenderMode.OVERLAY,
            closure = { spans.add(it) },
        )

        val colorSpan = spans.filterIsInstance<android.text.style.ForegroundColorSpan>().single()
        assertEquals(Color.RED, colorSpan.foregroundColor)
    }

    @Test
    fun baseRenderModeUsesTransparentStyleForAnimatedTextWithoutOutline() {
        val spans = mutableListOf<Any>()
        FontAttributes.default.copy(
            color = Color.RED,
            alignment = TextAlignment.LEFT,
            animationTransform = TextAnimationTransform(0f, 1.1f, 1f),
        ).enumerateSpans(
            fontManager = fontManager,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            renderMode = FontAttributes.RenderMode.BASE,
            suppressAnimatedBase = true,
            closure = { spans.add(it) },
        )

        assertTrue(spans.any { it is InvisibleForegroundColorSpan })
        assertFalse(spans.any { it is InvisibleReplacementSpan })
    }

    @Test
    fun activeNonRenderableAnimatedChunkStaysHidden() {
        val text = FakeAttributedText(
            listOf(
                Part(
                    content = "Hello",
                    animationTransform = TextAnimationTransform(0f, 1f, 0f),
                    color = Color.RED,
                ),
            )
        )

        val bitmap = drawOverlay(text)
        assertFalse(bitmapContainsVisiblePixel(bitmap))
    }

    @Test
    fun shouldAdjustSpacingUsesLineCenterForScaleOnlyAnimationWithoutThresholdJump() {
        val method = RichTextConverter::class.java.getDeclaredMethod(
            "shouldAdjustSpacing",
            TextAnimationTransform::class.java,
        )
        method.isAccessible = true

        assertEquals(
            true,
            method.invoke(converter, TextAnimationTransform(0f, 1.02f, 1f))
        )
        assertEquals(
            false,
            method.invoke(converter, TextAnimationTransform(0.02f, 1.2f, 1f))
        )
    }

    @Test
    fun applyAttributedTextUsesFastPathForUnchangedStaticText() {
        val context = getApplicationContext<Context>()
        val view = ValdiTextView(context)
        val helper = TextViewHelper(view, converter, FontAttributes.default, 0).also {
            it.fontAttributes = FontAttributes.default
        }
        val applyMethod = TextViewHelper::class.java.getDeclaredMethod(
            "applyAttributedText",
            AttributedText::class.java,
        )
        applyMethod.isAccessible = true
        val overlayField = TextViewHelper::class.java.getDeclaredField("overlayAttributedTextSpannable")
        overlayField.isAccessible = true
        val text = FakeAttributedText(
            listOf(
                Part("hello", null, color = Color.RED),
            )
        )

        applyMethod.invoke(helper, text)
        val firstOverlay = overlayField.get(helper)

        applyMethod.invoke(helper, text)
        val secondOverlay = overlayField.get(helper)

        assertNull(firstOverlay)
        assertNull(secondOverlay)
    }

    @Test
    fun rtlChunkBitmapLeftUsesChunkOriginWithoutDoubleApplyingLineLeft() {
        val context = getApplicationContext<Context>()
        val view = TextView(context)
        TextViewUtils.configure(view)
        view.textDirection = View.TEXT_DIRECTION_RTL
        val startingAttributes = FontAttributes.default.copy(alignment = TextAlignment.RIGHT)
        val text = FakeAttributedText(
            listOf(
                Part(
                    content = "\u0633\u0644\u0627\u0645",
                    animationTransform = TextAnimationTransform(0f, 1.2f, 1f),
                    color = Color.RED,
                ),
            )
        )
        val baseSpannable = converter.convert(
            attributedText = text,
            startingAttributes = startingAttributes,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.BASE,
            density = 1.0f,
        )
        val baseLayout = StaticLayout.Builder.obtain(
            baseSpannable,
            0,
            baseSpannable.length,
            TextPaint(view.paint),
            400,
        )
            .setAlignment(Layout.Alignment.ALIGN_OPPOSITE)
            .setIncludePad(view.includeFontPadding)
            .setTextDirection(TextViewUtils.resolveTextDirectionHeuristic(view))
            .build()
        val overlaySpannable = converter.convert(
            attributedText = text,
            startingAttributes = startingAttributes,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.OVERLAY,
            density = 1.0f,
        )
        val overlayLayoutCache = converter.buildOverlayLayoutCache(baseLayout, view, overlaySpannable)
        assertNotNull(overlayLayoutCache)
        val chunk = overlayLayoutCache!!.drawChunks.single()
        val bitmapPadding = kotlin.math.ceil(view.paint.textSize.toDouble()).toInt().coerceAtLeast(1)
        val expectedLeft = chunk.x - (chunk.bitmap.width - 2 * bitmapPadding) - bitmapPadding.toFloat()

        assertEquals(expectedLeft, chunk.bitmapLeft, 0.01f)
    }

    @Test
    fun ltrRunInRtlParagraphKeepsChunkOriginAtPrimaryHorizontal() {
        val context = getApplicationContext<Context>()
        val view = TextView(context)
        TextViewUtils.configure(view)
        view.textDirection = View.TEXT_DIRECTION_RTL
        val startingAttributes = FontAttributes.default.copy(alignment = TextAlignment.RIGHT)
        val text = FakeAttributedText(
            listOf(
                Part(
                    content = "abc",
                    animationTransform = TextAnimationTransform(0f, 1.2f, 1f),
                    color = Color.RED,
                ),
            )
        )
        val baseSpannable = converter.convert(
            attributedText = text,
            startingAttributes = startingAttributes,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.BASE,
            density = 1.0f,
        )
        val baseLayout = StaticLayout.Builder.obtain(
            baseSpannable,
            0,
            baseSpannable.length,
            TextPaint(view.paint),
            400,
        )
            .setAlignment(Layout.Alignment.ALIGN_OPPOSITE)
            .setIncludePad(view.includeFontPadding)
            .setTextDirection(TextViewUtils.resolveTextDirectionHeuristic(view))
            .build()
        val overlaySpannable = converter.convert(
            attributedText = text,
            startingAttributes = startingAttributes,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.OVERLAY,
            density = 1.0f,
        )
        val overlayLayoutCache = converter.buildOverlayLayoutCache(baseLayout, view, overlaySpannable)
        assertNotNull(overlayLayoutCache)
        val chunk = overlayLayoutCache!!.drawChunks.single()
        val bitmapPadding = kotlin.math.ceil(view.paint.textSize.toDouble()).toInt().coerceAtLeast(1)
        val expectedLeft = chunk.x - bitmapPadding.toFloat()

        assertEquals(expectedLeft, chunk.bitmapLeft, 0.01f)
    }

    @Test
    fun resolveLayoutOriginDoesNotDoubleApplyScrollOffsets() {
        val context = getApplicationContext<Context>()
        val view = TextView(context)
        TextViewUtils.configure(view)
        view.gravity = Gravity.TOP or Gravity.START
        view.measure(
            View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY),
        )
        view.layout(0, 0, 300, 200)
        view.scrollTo(17, 23)
        val layout = StaticLayout.Builder.obtain(
            "hello",
            0,
            5,
            TextPaint(view.paint),
            120,
        )
            .setAlignment(Layout.Alignment.ALIGN_NORMAL)
            .setIncludePad(view.includeFontPadding)
            .setTextDirection(TextViewUtils.resolveTextDirectionHeuristic(view))
            .build()
        val method = RichTextConverter::class.java.getDeclaredMethod(
            "resolveLayoutOrigin",
            TextView::class.java,
            Layout::class.java,
            PointF::class.java,
        )
        method.isAccessible = true
        val origin = PointF()

        method.invoke(converter, view, layout, origin)

        assertEquals(view.totalPaddingLeft.toFloat(), origin.x, 0.01f)
        assertEquals(view.extendedPaddingTop.toFloat(), origin.y, 0.01f)
    }

    @Test
    fun resolveLayoutOriginDoesNotApplyHorizontalGravityOffset() {
        val context = getApplicationContext<Context>()
        val view = TextView(context)
        TextViewUtils.configure(view)
        view.gravity = Gravity.CENTER_HORIZONTAL or Gravity.TOP
        view.measure(
            View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY),
        )
        view.layout(0, 0, 300, 200)
        val layout = StaticLayout.Builder.obtain(
            "hello",
            0,
            5,
            TextPaint(view.paint),
            120,
        )
            .setAlignment(Layout.Alignment.ALIGN_CENTER)
            .setIncludePad(view.includeFontPadding)
            .setTextDirection(TextViewUtils.resolveTextDirectionHeuristic(view))
            .build()
        val method = RichTextConverter::class.java.getDeclaredMethod(
            "resolveLayoutOrigin",
            TextView::class.java,
            Layout::class.java,
            PointF::class.java,
        )
        method.isAccessible = true
        val origin = PointF()

        method.invoke(converter, view, layout, origin)

        assertEquals(view.totalPaddingLeft.toFloat(), origin.x, 0.01f)
    }

    @Test
    fun valdiTextViewUpdateAndClearInvalidateView() {
        val view = ValdiTextView(getApplicationContext())
        val text = FakeAttributedText(listOf(Part("a", TextAnimationTransform(0f, 1.1f, 1f))))

        view.updateAttributedText(text)
        assertTrue(shadowOf(view).wasInvalidated())
        shadowOf(view).clearWasInvalidated()
        view.clearAttributedText()

        assertTrue(shadowOf(view).wasInvalidated())
    }

    private fun drawOverlay(text: FakeAttributedText): Bitmap {
        val context = getApplicationContext<Context>()
        val view = TextView(context)
        TextViewUtils.configure(view)
        view.textDirection = View.TEXT_DIRECTION_LTR

        val baseSpannable = converter.convert(
            attributedText = text,
            startingAttributes = FontAttributes.default,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.BASE,
            density = 1.0f,
        )
        val baseLayout = StaticLayout.Builder.obtain(
            baseSpannable,
            0,
            baseSpannable.length,
            TextPaint(view.paint),
            400,
        )
            .setAlignment(Layout.Alignment.ALIGN_NORMAL)
            .setIncludePad(view.includeFontPadding)
            .setTextDirection(TextViewUtils.resolveTextDirectionHeuristic(view))
            .build()

        val overlaySpannable = converter.convert(
            attributedText = text,
            startingAttributes = FontAttributes.default,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.OVERLAY,
            density = 1.0f,
        )
        val overlayLayoutCache = converter.buildOverlayLayoutCache(baseLayout, view, overlaySpannable)
        assertNotNull(overlayLayoutCache)

        view.measure(
            View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.AT_MOST),
        )
        view.layout(0, 0, 400, 200)

        val bitmap = Bitmap.createBitmap(400, 200, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        converter.drawOnTop(
            canvas = canvas,
            layout = baseLayout,
            overlayLayoutCache = overlayLayoutCache!!,
            view = view,
            parsedAttributedText = converter.parseAttributedText(text, FontAttributes.default),
            missingFontsTracker = missingFontsTracker,
        )
        return bitmap
    }

    private fun bitmapContainsVisiblePixel(bitmap: Bitmap): Boolean {
        for (x in 0 until bitmap.width) {
            for (y in 0 until bitmap.height) {
                if ((bitmap.getPixel(x, y) ushr 24) != 0) {
                    return true
                }
            }
        }
        return false
    }
}

private data class Part(
    val content: String,
    val animationTransform: TextAnimationTransform?,
    val color: Int? = null,
    val outlineColor: Int? = null,
    val outlineWidth: Float = 0f,
)

private class FakeAttributedText(private val parts: List<Part>) : AttributedText {
    override fun getPartsSize(): Int = parts.size
    override fun getContentAtIndex(index: Int): String = parts[index].content
    override fun getFontAtIndex(index: Int): String? = null
    override fun getTextDecorationAtIndex(index: Int): TextDecoration? = null
    override fun getColorAtIndex(index: Int): Int? = parts[index].color
    override fun getOnTapAtIndex(index: Int): ValdiFunction? = null
    override fun getOnLayoutAtIndex(index: Int): ValdiFunction? = null
    override fun getOutlineColorAtIndex(index: Int): Int? = parts[index].outlineColor
    override fun getOutlineWidthAtIndex(index: Int): Float = parts[index].outlineWidth
    override fun hasOutline(): Boolean = parts.any { it.outlineColor != null && it.outlineWidth > 0f }
    override fun getAnimationTransformAtIndex(index: Int): TextAnimationTransform? = parts[index].animationTransform
    override fun hasAnimationTransform(): Boolean = parts.any { it.animationTransform != null }
    override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? = null
}
