package com.snap.valdi.attributes.impl.richtext

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.PointF
import android.graphics.Typeface
import android.text.Spannable
import android.text.SpannableString
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.TextPaint
import android.text.Layout
import android.text.StaticLayout
import android.text.style.CharacterStyle
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
import com.snap.valdi.views.ValdiEditText
import com.snap.valdi.views.ValdiTextView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
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
    fun textValueSetterDoesNotBypassEqualityForStaticAttributedText() {
        val view = TextView(getApplicationContext())
        val helper = TextViewHelper(view, converter, FontAttributes.default, 0)
        val dirtyField = TextViewHelper::class.java.getDeclaredField("textValueDirty")
        dirtyField.isAccessible = true
        val text = FakeAttributedText(listOf(Part("a", null)))

        helper.textValue = text
        view.text = "a"
        dirtyField.set(helper, false)
        helper.textValue = text

        assertFalse(dirtyField.getBoolean(helper))
    }

    @Test
    fun textValueSetterRebindsSameStaticAttributedTextWhenViewTextDrifts() {
        val view = TextView(getApplicationContext())
        val helper = TextViewHelper(view, converter, FontAttributes.default, 0)
        val dirtyField = TextViewHelper::class.java.getDeclaredField("textValueDirty")
        dirtyField.isAccessible = true
        val text = FakeAttributedText(listOf(Part("a", null)))

        helper.textValue = text
        view.text = "b"
        dirtyField.set(helper, false)
        helper.textValue = text

        assertTrue(dirtyField.getBoolean(helper))
    }

    @Test
    fun valdiTextViewUpdateAttributedTextAcceptsSameRenderedContentAfterSetAttributedText() {
        val view = ValdiTextView(getApplicationContext())
        val initial = FakeAttributedText(listOf(Part("ab", null)))
        val updated = FakeAttributedText(listOf(Part("ab", TextAnimationTransform(0f, 1.1f, 1f))))

        view.setAttributedText(initial, SpannableString("ab"))
        view.updateAttributedText(updated)
    }

    @Test
    fun convertOnlyAddsPartIndexSpansForAnimatedText() {
        val staticText = FakeAttributedText(listOf(Part("a", null), Part("b", null)))
        val staticSpannable = converter.convert(
            attributedText = staticText,
            startingAttributes = FontAttributes.default,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.BASE,
            density = 1.0f,
        )

        assertTrue(
            staticSpannable.getSpans(0, staticSpannable.length, RichTextConverter.PartIndexSpan::class.java).isEmpty(),
        )

        val animatedText = FakeAttributedText(
            listOf(
                Part("a", TextAnimationTransform(0f, 1.1f, 1f)),
                Part("b", null),
            ),
        )
        val animatedSpannable = converter.convert(
            attributedText = animatedText,
            startingAttributes = FontAttributes.default,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.BASE,
            density = 1.0f,
        )

        assertEquals(
            2,
            animatedSpannable.getSpans(0, animatedSpannable.length, RichTextConverter.PartIndexSpan::class.java).size,
        )
    }

    @Test
    fun overlayConvertSkipsPartIndexSpanForImageAttachmentParts() {
        val text = FakeAttributedText(
            listOf(
                Part("a", TextAnimationTransform(0f, 1.1f, 1f)),
                Part("i", null, imageAttachment = ImageAttachmentInfo(10f, 10f, null)),
            ),
        )
        val overlaySpannable = converter.convert(
            attributedText = text,
            startingAttributes = FontAttributes.default,
            missingFontsTracker = missingFontsTracker,
            disableTextReplacement = false,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.OVERLAY,
            density = 1.0f,
        )
        val spans = overlaySpannable.getSpans(
            0,
            overlaySpannable.length,
            RichTextConverter.PartIndexSpan::class.java,
        )

        assertEquals(1, spans.size)
        assertEquals(0, spans.single().partIndex)
    }

    @Test
    fun isTextValueEqualMatchesImageAttachmentBreakCharacters() {
        val text = FakeAttributedText(
            listOf(
                Part("a", null, imageAttachment = ImageAttachmentInfo(10f, 10f, null)),
                Part("b", null),
            ),
        )

        assertTrue(TextViewHelper.isTextValueEqual(text, "a\u2009b"))
        assertFalse(TextViewHelper.isTextValueEqual(text, "ab"))
        assertTrue(TextViewHelper.isTextValueEqual(text, "ab", disableTextReplacement = true))
    }

    @Test
    fun overlayLayoutCacheRecycleRecyclesChunkBitmaps() {
        val bitmap = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888)
        val cache = RichTextConverter.OverlayLayoutCache(
            overlaySpannable = SpannableString("a"),
            width = 4,
            alignment = Layout.Alignment.ALIGN_NORMAL,
            lineSpacingExtra = 0f,
            lineSpacingMultiplier = 1f,
            includeFontPadding = false,
            breakStrategy = 0,
            hyphenationFrequency = 0,
            justificationMode = 0,
            textDirection = null,
            drawChunks = listOf(
                RichTextConverter.OverlayDrawChunk(
                    partIndex = 0,
                    startOffset = 0,
                    chunkText = "a",
                    x = 0f,
                    baselineY = 0f,
                    lineIndex = 0,
                    drawBounds = android.graphics.RectF(),
                    bitmap = bitmap,
                    bitmapLeft = 0f,
                    bitmapTop = 0f,
                ),
            ),
            drawChunksByPartIndex = emptyMap(),
            lineCenterByIndex = emptyMap(),
        )

        cache.recycle()

        assertTrue(bitmap.isRecycled)
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
    fun applyAttributedTextKeepsOverlayCacheForUnchangedStaticText() {
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

        assertSame(firstOverlay, secondOverlay)
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
    fun valdiTextViewUpdateAttributedTextRejectsChangedRenderedContent() {
        withDebuggableApp {
            val view = ValdiTextView(getApplicationContext())
            view.setAttributedText(
                FakeAttributedText(listOf(Part("old", null))),
                SpannableStringBuilder("old"),
            )

            assertFalse(view.updateAttributedText(FakeAttributedText(listOf(Part("newer", null)))))
        }
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

    @Test
    fun invisibleForegroundColorSpanSuppressesShadowLayer() {
        val context = getApplicationContext<Context>()
        val textView = TextView(context)
        textView.setTextColor(Color.WHITE)
        textView.setShadowLayer(4f, 2f, 2f, Color.BLACK)

        val spannable = SpannableString("a")
        spannable.setSpan(
            InvisibleForegroundColorSpan(),
            0,
            spannable.length,
            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE,
        )

        val layout = StaticLayout.Builder.obtain(
            spannable,
            0,
            spannable.length,
            TextPaint(textView.paint),
            200,
        )
            .setAlignment(Layout.Alignment.ALIGN_NORMAL)
            .setIncludePad(false)
            .setTextDirection(TextViewUtils.resolveTextDirectionHeuristic(textView))
            .build()

        val bitmap = Bitmap.createBitmap(200, 80, Bitmap.Config.ARGB_8888)
        layout.draw(Canvas(bitmap))

        assertFalse(bitmapContainsVisiblePixel(bitmap))
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

    private fun withDebuggableApp(block: () -> Unit) {
        val appInfo = getApplicationContext<Context>().applicationInfo
        val originalFlags = appInfo.flags
        appInfo.flags = originalFlags or ApplicationInfo.FLAG_DEBUGGABLE
        try {
            block()
        } finally {
            appInfo.flags = originalFlags
        }
    }

    // Flushes pending text/selection changes (the private apply pass driven by measure/layout).
    private fun flushTextAttributes(helper: TextViewHelper) {
        val method = TextViewHelper::class.java.getDeclaredMethod("updateTextAttributes")
        method.isAccessible = true
        method.invoke(helper)
    }

    private fun newEditTextHelper(matchIosTextSetCaret: Boolean): Pair<ValdiEditText, TextViewHelper> {
        val editText = ValdiEditText(getApplicationContext())
        val helper = TextViewHelper(editText, converter, FontAttributes.default, 0)
        editText.textViewHelper = helper
        helper.matchIosTextSetCaret = matchIosTextSetCaret
        return editText to helper
    }

    @Test
    fun editTextKeepsCaretAtEndAfterSetTextWhenSelectionClearedAndMatchIosOn() {
        val (editText, helper) = newEditTextHelper(matchIosTextSetCaret = true)

        helper.textValue = FakeAttributedText(listOf(Part("Hello", null)))
        flushTextAttributes(helper)

        assertEquals(5, editText.selectionStart)
        assertEquals(5, editText.selectionEnd)
    }

    @Test
    fun editTextKeepsCaretAtEndAfterPlainTextSetWhenSelectionClearedAndMatchIosOn() {
        val (editText, helper) = newEditTextHelper(matchIosTextSetCaret = true)

        // A plain String value goes through the non-attributed applyTextSimple path.
        helper.textValue = "Hello"
        flushTextAttributes(helper)

        assertEquals(5, editText.selectionStart)
        assertEquals(5, editText.selectionEnd)
    }

    @Test
    fun editTextDoesNotMoveCaretOnRebindWithoutSetText() {
        val (editText, helper) = newEditTextHelper(matchIosTextSetCaret = true)

        helper.textValue = FakeAttributedText(listOf(Part("Hello", null)))
        flushTextAttributes(helper)

        // User places the caret in the middle of the text.
        editText.setSelection(2)
        assertEquals(2, editText.selectionStart)

        // A new AttributedText instance with identical content arrives (an unrelated re-render).
        // This re-binds without calling setText, so the caret must be left untouched.
        helper.textValue = FakeAttributedText(listOf(Part("Hello", null)))
        flushTextAttributes(helper)

        assertEquals(2, editText.selectionStart)
        assertEquals(2, editText.selectionEnd)
    }

    @Test
    fun editTextRespectsExplicitSelectionOverEndWhenMatchIosOn() {
        val (editText, helper) = newEditTextHelper(matchIosTextSetCaret = true)

        helper.textValue = FakeAttributedText(listOf(Part("Hello", null)))
        helper.selection = Pair(1, 1)
        flushTextAttributes(helper)

        assertEquals(1, editText.selectionStart)
        assertEquals(1, editText.selectionEnd)
    }

    @Test
    fun editTextDoesNotForceCaretToEndWhenMatchIosOff() {
        val (editText, helper) = newEditTextHelper(matchIosTextSetCaret = false)

        helper.textValue = FakeAttributedText(listOf(Part("Hello", null)))
        flushTextAttributes(helper)

        // With the flag off we never force the caret to the end; Android leaves it at the start.
        assertEquals(0, editText.selectionStart)
    }

    @Test
    fun editTextReappliesExplicitSelectionAfterSetTextEvenWhenSelectionUnchanged() {
        val (editText, helper) = newEditTextHelper(matchIosTextSetCaret = true)

        helper.textValue = FakeAttributedText(listOf(Part("Hello", null)))
        helper.selection = Pair(1, 1)
        flushTextAttributes(helper)
        assertEquals(1, editText.selectionStart)

        // New text content forces setText (native caret -> 0), but the controlled selection value
        // is unchanged so it is not marked dirty. It must still be reapplied, not left at 0.
        helper.textValue = FakeAttributedText(listOf(Part("Goodbye", null)))
        helper.selection = Pair(1, 1)
        flushTextAttributes(helper)

        assertEquals(1, editText.selectionStart)
        assertEquals(1, editText.selectionEnd)
    }

    @Test
    fun setTextAndSelectionSkipsSetTextWhenPlainTextUnchanged() {
        val editText = ValdiEditText(getApplicationContext())

        editText.setTextAndSelection("Hello", 5, 5)
        val generationAfterFirstSet = editText.setTextGeneration

        // Same text, different caret: setText must be skipped (no caret churn), selection applied.
        editText.setTextAndSelection("Hello", 2, 2)

        assertEquals(generationAfterFirstSet, editText.setTextGeneration)
        assertEquals(2, editText.selectionStart)
    }

    @Test
    fun setTextAndSelectionClearsConverterSpansOnRichToPlainWithIdenticalString() {
        val (editText, helper) = newEditTextHelper(matchIosTextSetCaret = true)

        // Rich text -> the converter applies styling spans and marks the view as attributed.
        helper.textValue = FakeAttributedText(listOf(Part("Hello", null, color = Color.RED)))
        flushTextAttributes(helper)
        val generationAfterRich = editText.setTextGeneration

        // Switch to plain text with the identical visible string. setText must still run so the
        // leftover converter spans are cleared, even though the string is unchanged.
        helper.textValue = "Hello"
        flushTextAttributes(helper)

        assertNotEquals(generationAfterRich, editText.setTextGeneration)
        val text = requireNotNull(editText.text)
        assertTrue(text.getSpans(0, text.length, CharacterStyle::class.java).isEmpty())
    }
}

private data class Part(
    val content: String,
    val animationTransform: TextAnimationTransform?,
    val color: Int? = null,
    val outlineColor: Int? = null,
    val outlineWidth: Float = 0f,
    val imageAttachment: ImageAttachmentInfo? = null,
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
    override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? = parts[index].imageAttachment
}
