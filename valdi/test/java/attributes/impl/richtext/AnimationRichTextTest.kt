package com.snap.valdi.attributes.impl.richtext

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.text.SpannableString
import android.text.Spanned
import android.text.StaticLayout
import android.text.TextPaint
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.Logger
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class AnimationRichTextTest {
    private lateinit var fontManager: FontManager
    private lateinit var missingFontsTracker: MissingFontsTracker

    @Before
    fun setUp() {
        val appInfo = getApplicationContext<Context>().applicationInfo
        appInfo.targetSdkVersion = 28
        appInfo.flags = appInfo.flags or ApplicationInfo.FLAG_SUPPORTS_RTL

        fontManager = createFontManager(getApplicationContext())
        missingFontsTracker = object : MissingFontsTracker {
            override fun onFontMissing(fontDescriptor: FontDescriptor) = Unit
        }
    }

    @Test
    fun activeAnimationCanBeNonRenderable() {
        val transform = textAnimationTransform(0f, 1f, 0f, null)

        assertTrue(isActiveAnimationTransform(transform))
        assertFalse(isRenderableAnimationTransform(transform))
    }

    @Test
    fun attributedTextReportsActiveAnimation() {
        val text = FakeAttributedText(
            listOf(
                plainPart("a"),
                animatedPart("b", textAnimationTransform(0f, 1f, 0f, null)),
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
    fun processedTextOnlyAddsAnimationSpansForAnimatedText() {
        val staticProcessedText = parse(FakeAttributedText(listOf(plainPart("a"), plainPart("b"))))

        assertFalse(staticProcessedText.hasAnimationTransform)
        assertEquals(
            0,
            staticProcessedText.spannable.getSpans(
                0,
                staticProcessedText.spannable.length,
                AnimatedTextReplacementSpan::class.java,
            ).size,
        )

        val animatedProcessedText = parseWithAnimator(
            FakeAttributedText(
                listOf(
                    animatedPart("a", textAnimationTransform(0f, 1.1f, 1f, null)),
                    plainPart("b"),
                )
            )
        )

        assertTrue(animatedProcessedText.hasAnimationTransform)
        assertEquals(1, animatedProcessedText.animationTransformsCount)
        assertEquals(
            1,
            animatedProcessedText.spannable.getSpans(
                0,
                animatedProcessedText.spannable.length,
                AnimatedTextReplacementSpan::class.java,
            ).size,
        )
    }

    @Test
    fun processedTextSplitsAnimationByPartPattern() {
        val processedText = parseWithAnimator(
            FakeAttributedText(
                listOf(
                    animatedPart("abc", textAnimationTransform(0f, 1.1f, 1f, "."))
                )
            )
        )
        val animationRanges = processedText.animationTransforms!!

        assertEquals(3, processedText.animationTransformsCount)
        assertEquals(listOf(0, 1, 2), animationRanges.map { it.start })
        assertEquals(listOf(1, 2, 3), animationRanges.map { it.end })
        assertEquals(listOf(0, 1, 2), animationRanges.map { it.value.startTransform.partIndexInGroup })
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
            .setAlignment(android.text.Layout.Alignment.ALIGN_NORMAL)
            .setIncludePad(false)
            .build()

        val bitmap = Bitmap.createBitmap(200, 80, Bitmap.Config.ARGB_8888)
        layout.draw(Canvas(bitmap))

        assertFalse(bitmapContainsVisiblePixel(bitmap))
    }

    private fun parse(text: FakeAttributedText): ValdiProcessedText {
        return ValdiProcessedText.parse(
            fontManager,
            text,
            FontAttributes.default,
            missingFontsTracker,
            NoopLogger,
        )
    }

    private fun parseWithAnimator(text: FakeAttributedText): ValdiProcessedText {
        val animator = AttributedTextAnimator()
        animator.beginSync()
        return try {
            ValdiProcessedText.parse(
                fontManager,
                text,
                FontAttributes.default,
                missingFontsTracker,
                NoopLogger,
                attributedTextAnimator = animator,
            )
        } finally {
            animator.endSync()
        }
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

private object NoopLogger : Logger {
    override fun log(level: Int, message: String?) = Unit
    override fun log(level: Int, err: Throwable?, message: String?) = Unit
}

private fun createFontManager(context: Context): FontManager {
    return FontManager(context, object : TypefaceResLoader {
        override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
    })
}

private fun textAnimationTransform(
    translationY: Float,
    scale: Float,
    opacity: Float,
    partPattern: String?
): TextAnimationTransform {
    return TextAnimationTransform(
        key = "intro",
        translationY = translationY,
        scale = scale,
        opacity = opacity,
        duration = 0.25,
        timeOffsetBetweenParts = 0.0,
        groupIndex = 0,
        partIndexInGroup = 0,
        partPattern = partPattern,
    )
}

private fun plainPart(content: String): Part {
    return Part(content, null, null, null, 0f, null, null)
}

private fun animatedPart(content: String, animationTransform: TextAnimationTransform): Part {
    return Part(content, animationTransform, null, null, 0f, null, null)
}

private data class Part(
    val content: String,
    val animationTransform: TextAnimationTransform?,
    val color: Int?,
    val outlineColor: Int?,
    val outlineWidth: Float,
    val imageAttachment: ImageAttachmentInfo?,
    val inlineViewAttachment: InlineViewAttachmentInfo?,
)

private class FakeAttributedText(private val parts: List<Part>) : AttributedText {
    override fun getPartsSize(): Int = parts.size
    override fun getContentAtIndex(index: Int): String = parts[index].content
    override fun getFontAtIndex(index: Int): String? = null
    override fun getTextDecorationAtIndex(index: Int): TextDecoration? = null
    override fun getColorAtIndex(index: Int): Int? = parts[index].color
    override fun getBackgroundColorAtIndex(index: Int): Int? = null
    override fun getOnTapAtIndex(index: Int): ValdiFunction? = null
    override fun getOnLayoutAtIndex(index: Int): ValdiFunction? = null
    override fun getOutlineColorAtIndex(index: Int): Int? = parts[index].outlineColor
    override fun getOutlineWidthAtIndex(index: Int): Float = parts[index].outlineWidth
    override fun hasOutline(): Boolean = parts.any { it.outlineColor != null && it.outlineWidth > 0f }
    override fun getAnimationTransformsSize(): Int = parts.count { it.animationTransform != null }
    override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? = parts[index].imageAttachment
    override fun getInlineViewAttachmentAtIndex(index: Int): InlineViewAttachmentInfo? = parts[index].inlineViewAttachment
    override fun getAnimationTransformAtIndex(index: Int): TextAnimationTransform? = parts[index].animationTransform
}
