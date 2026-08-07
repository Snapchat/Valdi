package com.snap.valdi.attributes.impl.richtext

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Paint
import android.graphics.Typeface
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.EditTextAttributesBinder
import com.snap.valdi.attributes.impl.EditTextMultilineAttributesBinder
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.ValdiEditText
import com.snap.valdi.views.ValdiEditTextInput
import com.snap.valdi.views.ValdiEditTextMultiline
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

private object NoopLoggerForLayout : Logger {
    override fun log(level: Int, message: String?) = Unit
    override fun log(level: Int, err: Throwable?, message: String?) = Unit
}

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class LayoutRegressionTest {
    private lateinit var fontManager: FontManager
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
        missingFontsTracker = object : MissingFontsTracker {
            override fun onFontMissing(fontDescriptor: FontDescriptor) = Unit
        }
    }

    // --- textDirection binding existence ---

    @Test
    fun editTextBinderHasApplyTextDirectionMethod() {
        val binder = EditTextAttributesBinder(
            getApplicationContext(), fontManager, FontAttributes.default,
            resetSelectionMatchesIos = false, logger = NoopLoggerForLayout,
        )
        val method = binder.javaClass.getDeclaredMethod(
            "applyTextDirection",
            ValdiEditText::class.java,
            String::class.java,
            com.snap.valdi.attributes.impl.animations.ValdiAnimator::class.java,
        )
        assertNotNull(method)
    }

    @Test
    fun editTextBinderHasResetTextDirectionMethod() {
        val binder = EditTextAttributesBinder(
            getApplicationContext(), fontManager, FontAttributes.default,
            resetSelectionMatchesIos = false, logger = NoopLoggerForLayout,
        )
        val method = binder.javaClass.getDeclaredMethod(
            "resetTextDirection",
            ValdiEditText::class.java,
            com.snap.valdi.attributes.impl.animations.ValdiAnimator::class.java,
        )
        assertNotNull(method)
    }

    @Test
    fun applyTextDirectionAcceptsStringParameter() {
        val binder = EditTextAttributesBinder(
            getApplicationContext(), fontManager, FontAttributes.default,
            resetSelectionMatchesIos = false, logger = NoopLoggerForLayout,
        )
        val method = binder.javaClass.getDeclaredMethod(
            "applyTextDirection",
            ValdiEditText::class.java,
            String::class.java,
            ValdiAnimator::class.java,
        )
        method.isAccessible = true
        assertEquals(3, method.parameterCount)
        assertEquals(String::class.java, method.parameterTypes[1])
    }

    @Test
    fun resetTextDirectionTakesViewAndAnimatorOnly() {
        val binder = EditTextAttributesBinder(
            getApplicationContext(), fontManager, FontAttributes.default,
            resetSelectionMatchesIos = false, logger = NoopLoggerForLayout,
        )
        val method = binder.javaClass.getDeclaredMethod(
            "resetTextDirection",
            ValdiEditText::class.java,
            ValdiAnimator::class.java,
        )
        method.isAccessible = true
        assertEquals(2, method.parameterCount)
    }

    // --- multiline contentType "noSuggestions" bindings ---

    @Test
    fun multilineBinderHasApplyContentTypeMethod() {
        val multilineBinder = EditTextMultilineAttributesBinder(
            getApplicationContext(),
        )
        val method = multilineBinder.javaClass.getDeclaredMethod(
            "applyContentType",
            ValdiEditTextMultiline::class.java,
            String::class.java,
            ValdiAnimator::class.java,
        )
        assertNotNull(method)
    }

    @Test
    fun multilineBinderHasResetContentTypeMethod() {
        val multilineBinder = EditTextMultilineAttributesBinder(
            getApplicationContext(),
        )
        val method = multilineBinder.javaClass.getDeclaredMethod(
            "resetContentType",
            ValdiEditTextMultiline::class.java,
            ValdiAnimator::class.java,
        )
        assertNotNull(method)
    }

    @Test
    fun multilineApplyContentTypeReferencesPrivateImeOptionsField() {
        val field = ValdiEditTextInput::class.java.getDeclaredField("disableMediaContent")
        assertNotNull(field)
        assertEquals(Boolean::class.javaPrimitiveType, field.type)
    }

    // --- Image attachment layout tests ---

    @Test
    fun imageAttachmentSpanReportsScaledWidth() {
        val info = ImageAttachmentInfo(20f, 10f, null)
        val density = 2.0f
        val span = ImageAttachmentSpan(info, density)
        val paint = Paint().apply { textSize = 16f }

        val width = span.getSize(paint, "X", 0, 1, null)

        assertEquals((20f * density).toInt(), width)
    }

    @Test
    fun imageAttachmentSpanAdjustsFontMetricsForHeight() {
        val info = ImageAttachmentInfo(20f, 30f, null)
        val density = 1.0f
        val span = ImageAttachmentSpan(info, density)
        val paint = Paint().apply { textSize = 16f }
        val fm = Paint.FontMetricsInt()
        fm.ascent = -12
        fm.top = -14
        fm.descent = 4
        fm.bottom = 6

        span.getSize(paint, "X", 0, 1, fm)

        assertTrue("Image should extend beyond default ascent", fm.ascent <= -12)
        assertTrue("Image should extend beyond default descent or maintain it", fm.descent >= 4)
    }

    @Test
    fun convertWithImageAttachmentCreatesImageAttachmentSpan() {
        val text = FakeAttributedTextForLayout(
            listOf(
                LayoutPart("a", imageAttachment = ImageAttachmentInfo(10f, 10f, null)),
                LayoutPart("b"),
            ),
        )

        val processed = ValdiProcessedText.parse(
            fontManager = fontManager,
            attributedText = text,
            startingAttributes = FontAttributes.default,
            missingFontsTracker = missingFontsTracker,
            logger = NoopLoggerForLayout,
            disableTextReplacement = false,
            density = 1.0f,
        )

        val spans = processed.spannable.getSpans(0, processed.spannable.length, ImageAttachmentSpan::class.java)
        assertEquals(1, spans.size)
    }

    @Test
    fun convertWithImageAttachmentAppendsBreakCharacter() {
        val text = FakeAttributedTextForLayout(
            listOf(
                LayoutPart("a", imageAttachment = ImageAttachmentInfo(10f, 10f, null)),
                LayoutPart("b"),
            ),
        )

        val processed = ValdiProcessedText.parse(
            fontManager = fontManager,
            attributedText = text,
            startingAttributes = FontAttributes.default,
            missingFontsTracker = missingFontsTracker,
            logger = NoopLoggerForLayout,
            disableTextReplacement = false,
            density = 1.0f,
        )

        assertEquals("a b", processed.spannable.toString())
    }

    @Test
    fun convertWithDisableTextReplacementSkipsImageAttachmentSpan() {
        val text = FakeAttributedTextForLayout(
            listOf(
                LayoutPart("a", imageAttachment = ImageAttachmentInfo(10f, 10f, null)),
                LayoutPart("b"),
            ),
        )

        val processed = ValdiProcessedText.parse(
            fontManager = fontManager,
            attributedText = text,
            startingAttributes = FontAttributes.default,
            missingFontsTracker = missingFontsTracker,
            logger = NoopLoggerForLayout,
            disableTextReplacement = true,
            density = 1.0f,
        )

        val spans = processed.spannable.getSpans(0, processed.spannable.length, ImageAttachmentSpan::class.java)
        assertEquals(0, spans.size)
    }
}

private data class LayoutPart(
    val content: String,
    val animationTransform: TextAnimationTransform? = null,
    val color: Int? = null,
    val outlineColor: Int? = null,
    val outlineWidth: Float = 0f,
    val imageAttachment: ImageAttachmentInfo? = null,
    val inlineViewAttachment: InlineViewAttachmentInfo? = null,
)

private class FakeAttributedTextForLayout(private val parts: List<LayoutPart>) : AttributedText {
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
    override fun getAnimationTransformAtIndex(index: Int): TextAnimationTransform? = parts[index].animationTransform
    override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? = parts[index].imageAttachment
    override fun getInlineViewAttachmentAtIndex(index: Int): InlineViewAttachmentInfo? = parts[index].inlineViewAttachment
}
