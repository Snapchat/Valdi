package com.snap.valdi.attributes

import android.content.Context
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.text.style.ForegroundColorSpan
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.attributes.impl.richtext.AttributedText
import com.snap.valdi.attributes.impl.richtext.AttributedTextAnimator
import com.snap.valdi.attributes.impl.richtext.AnimatedTextReplacementSpan
import com.snap.valdi.attributes.impl.richtext.CustomUnderlineSpan
import com.snap.valdi.attributes.impl.richtext.CustomUnderlineStyle
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.ImageAttachmentInfo
import com.snap.valdi.attributes.impl.richtext.InlineViewAttachmentInfo
import com.snap.valdi.attributes.impl.richtext.InlineViewVerticalAlignment
import com.snap.valdi.attributes.impl.richtext.OutlineReplacementSpan
import com.snap.valdi.attributes.impl.richtext.TextAnimationTransform
import com.snap.valdi.attributes.impl.richtext.TextDecoration
import com.snap.valdi.attributes.impl.richtext.TextSizeSpan
import com.snap.valdi.attributes.impl.richtext.ValdiProcessedText
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.ValdiMarshaller
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class ValdiProcessedTextTest {
    private class Part(
        val content: String,
        val font: String? = null,
        val textDecoration: TextDecoration? = null,
        val color: Int? = null,
        val backgroundColor: Int? = null,
        val onTap: ValdiFunction? = null,
        val onLayout: ValdiFunction? = null,
        val outlineColor: Int? = null,
        val outlineWidth: Float = 0f,
        val imageAttachment: ImageAttachmentInfo? = null,
        var inlineViewAttachment: InlineViewAttachmentInfo? = null,
        val animationTransform: TextAnimationTransform? = null
    )

    private class FakeAttributedText(private val parts: List<Part>) : AttributedText {
        override fun getPartsSize(): Int = parts.size
        override fun getContentAtIndex(index: Int): String = parts[index].content
        override fun getFontAtIndex(index: Int): String? = parts[index].font
        override fun getTextDecorationAtIndex(index: Int): TextDecoration? = parts[index].textDecoration
        override fun getColorAtIndex(index: Int): Int? = parts[index].color
        override fun getBackgroundColorAtIndex(index: Int): Int? = parts[index].backgroundColor
        override fun getOnTapAtIndex(index: Int): ValdiFunction? = parts[index].onTap
        override fun getOnLayoutAtIndex(index: Int): ValdiFunction? = parts[index].onLayout
        override fun getOutlineColorAtIndex(index: Int): Int? = parts[index].outlineColor
        override fun getOutlineWidthAtIndex(index: Int): Float = parts[index].outlineWidth
        override fun hasOutline(): Boolean = parts.any { it.outlineColor != null && it.outlineWidth > 0f }
        override fun getAnimationTransformsSize(): Int = parts.count { it.animationTransform != null }
        override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? = parts[index].imageAttachment
        override fun getInlineViewAttachmentAtIndex(index: Int): InlineViewAttachmentInfo? = parts[index].inlineViewAttachment
        override fun getAnimationTransformAtIndex(index: Int): TextAnimationTransform? = parts[index].animationTransform
    }

    private class FakeValdiFunction : ValdiFunction {
        override fun perform(marshaller: ValdiMarshaller): Boolean = true
    }

    private class NoopMissingFontsTracker : MissingFontsTracker {
        override fun onFontMissing(fontDescriptor: FontDescriptor) {}
    }

    private object NoopLogger : Logger {
        override fun log(level: Int, message: String?) = Unit
        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    private data class LogEntry(
        val level: Int,
        val err: Throwable?,
        val message: String?
    )

    private class CaptureLogger : Logger {
        val entries = mutableListOf<LogEntry>()

        override fun log(level: Int, message: String?) {
            entries.add(LogEntry(level, null, message))
        }

        override fun log(level: Int, err: Throwable?, message: String?) {
            entries.add(LogEntry(level, err, message))
        }
    }

    private fun createFontManager(context: Context): FontManager {
        return FontManager(context, object : TypefaceResLoader {
            override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
        })
    }

    private fun parseWithAnimator(parts: List<Part>): ValdiProcessedText {
        return parseWithAnimator(parts, NoopLogger)
    }

    private fun parseWithAnimator(parts: List<Part>, logger: Logger): ValdiProcessedText {
        val animator = AttributedTextAnimator()
        animator.beginSync()
        return try {
            ValdiProcessedText.parse(
                createFontManager(getApplicationContext()),
                FakeAttributedText(parts),
                FontAttributes.default,
                NoopMissingFontsTracker(),
                logger,
                attributedTextAnimator = animator
            )
        } finally {
            animator.endSync()
        }
    }

    @Test
    fun parseIndexesCallbacksAndInlineAttachments() {
        val onTap = FakeValdiFunction()
        val onLayout = FakeValdiFunction()
        val inlineAttachment = InlineViewAttachmentInfo(
            childIndex = 2,
            verticalAlignment = InlineViewVerticalAlignment.Baseline,
            width = 12f,
            height = 8f
        )
        val processedText = ValdiProcessedText.parse(
            createFontManager(getApplicationContext()),
            FakeAttributedText(
                listOf(
                    Part("Hello ", onTap = onTap),
                    Part("chip", inlineViewAttachment = inlineAttachment),
                    Part(" world", onLayout = onLayout)
                )
            ),
            FontAttributes.default,
            NoopMissingFontsTracker(),
            NoopLogger,
            density = 2f
        )

        assertEquals("Hello chip\u2009 world", processedText.spannable.toString())
        assertTrue(processedText.hasOnTap)
        assertTrue(processedText.hasOnLayout)
        assertTrue(processedText.hasInlineViewAttachment)
        assertFalse(processedText.hasAnimationTransform)
        assertSame(onTap, processedText.onTapAtIndex(1)?.value)
        assertSame(onLayout, processedText.onLayoutAtIndex("Hello chip\u2009 ".length)?.value)
        assertFalse(processedText.hasInlineViewAttachmentForIndex(0))
        assertFalse(processedText.hasInlineViewAttachmentForIndex(1))
        assertTrue(processedText.hasInlineViewAttachmentForIndex(2))
        assertFalse(processedText.hasInlineViewAttachmentForIndex(3))
        assertEquals(null, processedText.inlineViewAttachmentForViewIndex(0))
        assertSame(inlineAttachment, processedText.inlineViewAttachmentForViewIndex(2)?.value?.attachmentInfo)

        val inlineItem = processedText.inlineViewAttachmentAtIndex("Hello ".length)
        assertEquals(6, inlineItem?.start)
        assertEquals(10, inlineItem?.end)
        assertSame(inlineAttachment, inlineItem?.value?.attachmentInfo)
        assertEquals(24, inlineItem?.value?.layoutSize?.width)
        assertEquals(16, inlineItem?.value?.layoutSize?.height)
    }

    @Test
    fun parseKeepsRenderingSpansOnMutableSpannable() {
        val processedText = ValdiProcessedText.parse(
            createFontManager(getApplicationContext()),
            FakeAttributedText(listOf(Part("colored", color = Color.RED))),
            FontAttributes.default,
            NoopMissingFontsTracker(),
            NoopLogger
        )

        val colorSpans = processedText.spannable.getSpans(0, processedText.spannable.length, ForegroundColorSpan::class.java)
        assertEquals(1, colorSpans.size)
        assertEquals(Color.RED, colorSpans[0].foregroundColor)
    }

    @Test
    fun parseKeepsStylingSpansOnInlineViewAttachments() {
        val context = getApplicationContext<Context>()
        val processedText = ValdiProcessedText.parse(
            createFontManager(context),
            FakeAttributedText(
                listOf(
                    Part(
                        "chip",
                        font = "system 24",
                        color = Color.RED,
                        inlineViewAttachment = InlineViewAttachmentInfo(
                            childIndex = 0,
                            verticalAlignment = InlineViewVerticalAlignment.Top,
                            width = 12f,
                            height = 8f
                        )
                    )
                )
            ),
            FontAttributes.default,
            NoopMissingFontsTracker(),
            NoopLogger
        )

        val inlineItem = processedText.inlineViewAttachments!![0]
        val textSizeSpans = processedText.spannable.getSpans(inlineItem.start, inlineItem.end, TextSizeSpan::class.java)
        val colorSpans = processedText.spannable.getSpans(inlineItem.start, inlineItem.end, ForegroundColorSpan::class.java)

        assertEquals(1, textSizeSpans.size)
        assertEquals(1, colorSpans.size)
        assertEquals(Color.RED, colorSpans[0].foregroundColor)
    }

    @Test
    fun inlineViewAttachmentSpanCachesMeasuredFontMetrics() {
        val processedText = ValdiProcessedText.parse(
            createFontManager(getApplicationContext()),
            FakeAttributedText(
                listOf(
                    Part(
                        "chip",
                        inlineViewAttachment = InlineViewAttachmentInfo(
                            childIndex = 0,
                            verticalAlignment = InlineViewVerticalAlignment.Bottom,
                            width = 12f,
                            height = 8f
                        )
                    )
                )
            ),
            FontAttributes.default,
            NoopMissingFontsTracker(),
            NoopLogger
        )
        val span = processedText.inlineViewAttachments!![0].value
        val paint = Paint()
        paint.textSize = 42f

        span.getSize(paint, processedText.spannable, 0, 4, Paint.FontMetricsInt())

        val measuredFontMetrics = span.fontMetrics!!
        assertEquals(paint.fontMetrics.ascent, measuredFontMetrics.ascent, 0.01f)
        assertEquals(paint.fontMetrics.descent, measuredFontMetrics.descent, 0.01f)
    }

    @Test
    fun parseIndexesAnimationTransforms() {
        val animator = AttributedTextAnimator()
        animator.beginSync()
        val processedText = try {
            ValdiProcessedText.parse(
                createFontManager(getApplicationContext()),
                FakeAttributedText(
                    listOf(
                        Part(
                            "animated",
                            animationTransform = TextAnimationTransform(
                                key = "intro",
                                translationY = 4f,
                                scale = 0.5f,
                                opacity = 0f,
                                duration = 0.25,
                                timeOffsetBetweenParts = 0.0,
                                groupIndex = 0,
                                partIndexInGroup = 0,
                                partPattern = null
                            )
                        )
                    )
                ),
                FontAttributes.default,
                NoopMissingFontsTracker(),
                NoopLogger,
                attributedTextAnimator = animator
            )
        } finally {
            animator.endSync()
        }

        assertTrue(processedText.hasAnimationTransform)
        assertEquals(1, processedText.animationTransformsCount)
        val animationTransforms = processedText.animationTransforms!!
        assertEquals(0, animationTransforms[0].start)
        assertEquals("animated".length, animationTransforms[0].end)
    }

    @Test
    fun parseIndexesCharacterPartPatternAnimationTransforms() {
        val processedText = parseWithAnimator(
            listOf(
                Part(
                    "abc",
                    animationTransform = TextAnimationTransform(
                        key = "chars",
                        translationY = 4f,
                        scale = 0.5f,
                        opacity = 0f,
                        duration = 0.25,
                        timeOffsetBetweenParts = 0.1,
                        groupIndex = 0,
                        partIndexInGroup = 0,
                        partPattern = "."
                    )
                )
            )
        )

        assertEquals(3, processedText.animationTransformsCount)
        val animationTransforms = processedText.animationTransforms!!
        for (index in 0 until 3) {
            assertEquals(index, animationTransforms[index].start)
            assertEquals(index + 1, animationTransforms[index].end)
            assertEquals("chars:${index + 1}", animationTransforms[index].value.key)
            assertEquals(index, animationTransforms[index].value.startTransform.partIndexInGroup)
        }
    }

    @Test
    fun parseIndexesWordPartPatternAnimationTransforms() {
        val processedText = parseWithAnimator(
            listOf(
                Part(
                    "hi there",
                    animationTransform = TextAnimationTransform(
                        key = "words",
                        translationY = 4f,
                        scale = 0.5f,
                        opacity = 0f,
                        duration = 0.25,
                        timeOffsetBetweenParts = 0.1,
                        groupIndex = 0,
                        partIndexInGroup = 0,
                        partPattern = "\\S+"
                    )
                )
            )
        )

        assertEquals(2, processedText.animationTransformsCount)
        val animationTransforms = processedText.animationTransforms!!
        assertEquals(0, animationTransforms[0].start)
        assertEquals(2, animationTransforms[0].end)
        assertEquals(3, animationTransforms[1].start)
        assertEquals(8, animationTransforms[1].end)
        assertEquals(0, animationTransforms[0].value.startTransform.partIndexInGroup)
        assertEquals(1, animationTransforms[1].value.startTransform.partIndexInGroup)
    }

    @Test
    fun parseLeavesAnimatedCustomUnderlineSpansToReplacementSpansAcrossPartPattern() {
        val animator = AttributedTextAnimator()
        animator.beginSync()
        val processedText = try {
            ValdiProcessedText.parse(
                createFontManager(getApplicationContext()),
                FakeAttributedText(
                    listOf(
                        Part(
                            "hi there",
                            animationTransform = TextAnimationTransform(
                                key = "underlined-words",
                                translationY = 4f,
                                scale = 0.5f,
                                opacity = 0f,
                                duration = 0.25,
                                timeOffsetBetweenParts = 0.1,
                                groupIndex = 0,
                                partIndexInGroup = 0,
                                partPattern = "\\S+"
                            )
                        )
                    )
                ),
                FontAttributes.default.copy(
                    textDecoration = TextDecoration.UNDERLINE,
                    customUnderlineStyle = CustomUnderlineStyle(1f, 1f, 1f, -2f)
                ),
                NoopMissingFontsTracker(),
                NoopLogger,
                attributedTextAnimator = animator
            )
        } finally {
            animator.endSync()
        }

        val underlineSpans = processedText.spannable.getSpans(
            0,
            processedText.spannable.length,
            CustomUnderlineSpan::class.java
        )
        val underlineRanges = underlineSpans
            .map { processedText.spannable.getSpanStart(it) to processedText.spannable.getSpanEnd(it) }
            .sortedBy { it.first }
        val replacementSpans = processedText.spannable.getSpans(
            0,
            processedText.spannable.length,
            AnimatedTextReplacementSpan::class.java
        )
        val replacementRanges = replacementSpans
            .map { processedText.spannable.getSpanStart(it) to processedText.spannable.getSpanEnd(it) }
            .sortedBy { it.first }

        assertEquals(listOf(2 to 3), underlineRanges)
        assertEquals(listOf(0 to 2, 3 to 8), replacementRanges)
    }

    @Test
    fun parseIndexesInvalidPartPatternLogsAndSkipsAnimationRanges() {
        val logger = CaptureLogger()
        val processedText = parseWithAnimator(
            listOf(
                Part(
                    "abc",
                    animationTransform = TextAnimationTransform(
                        key = "invalid",
                        translationY = 4f,
                        scale = 0.5f,
                        opacity = 0f,
                        duration = 0.25,
                        timeOffsetBetweenParts = 0.1,
                        groupIndex = 0,
                        partIndexInGroup = 0,
                        partPattern = "["
                    )
                )
            ),
            logger
        )

        assertFalse(processedText.hasAnimationTransform)
        assertEquals(0, processedText.animationTransformsCount)
        assertEquals(1, logger.entries.size)
        assertEquals(LogLevel.ERROR, logger.entries[0].level)
        assertTrue(logger.entries[0].err is IllegalArgumentException)
        assertTrue(logger.entries[0].message!!.contains("Invalid text animation partPattern"))
    }

    @Test
    fun parseIndexesPartPatternKeepsGroupIndexesContinuousAcrossParts() {
        val processedText = parseWithAnimator(
            listOf(
                Part(
                    "ab",
                    animationTransform = TextAnimationTransform(
                        key = "group",
                        translationY = 4f,
                        scale = 0.5f,
                        opacity = 0f,
                        duration = 0.25,
                        timeOffsetBetweenParts = 0.1,
                        groupIndex = 7,
                        partIndexInGroup = 0,
                        partPattern = "."
                    )
                ),
                Part(
                    "cd",
                    animationTransform = TextAnimationTransform(
                        key = "group",
                        translationY = 4f,
                        scale = 0.5f,
                        opacity = 0f,
                        duration = 0.25,
                        timeOffsetBetweenParts = 0.1,
                        groupIndex = 7,
                        partIndexInGroup = 99,
                        partPattern = null
                    )
                )
            )
        )

        assertEquals(3, processedText.animationTransformsCount)
        val animationTransforms = processedText.animationTransforms!!
        assertEquals(0, animationTransforms[0].value.startTransform.partIndexInGroup)
        assertEquals(1, animationTransforms[1].value.startTransform.partIndexInGroup)
        assertEquals(2, animationTransforms[2].value.startTransform.partIndexInGroup)
        assertEquals(0, animationTransforms[0].start)
        assertEquals(1, animationTransforms[1].start)
        assertEquals(2, animationTransforms[2].start)
        assertEquals(4, animationTransforms[2].end)
    }

    @Test
    fun parseIndexesOutlinedPartPatternDoesNotOverlapReplacementSpans() {
        val processedText = parseWithAnimator(
            listOf(
                Part(
                    "hi there",
                    outlineColor = Color.RED,
                    outlineWidth = 2f,
                    animationTransform = TextAnimationTransform(
                        key = "outline",
                        translationY = 4f,
                        scale = 0.5f,
                        opacity = 0f,
                        duration = 0.25,
                        timeOffsetBetweenParts = 0.1,
                        groupIndex = 0,
                        partIndexInGroup = 0,
                        partPattern = "\\S+"
                    )
                )
            )
        )

        val animatedSpans = processedText.spannable.getSpans(
            0,
            processedText.spannable.length,
            AnimatedTextReplacementSpan::class.java
        )
        val outlineReplacementSpans = processedText.spannable.getSpans(
            0,
            processedText.spannable.length,
            OutlineReplacementSpan::class.java
        )
        assertEquals(2, animatedSpans.size)
        assertEquals(0, outlineReplacementSpans.size)
    }

    @Test
    fun parseIndexesInlineViewAnimationTransformsWithoutAnimatedReplacementSpan() {
        val inlineAttachment = InlineViewAttachmentInfo(
            childIndex = 0,
            verticalAlignment = InlineViewVerticalAlignment.Center,
            width = 12f,
            height = 8f
        )
        val animator = AttributedTextAnimator()
        animator.beginSync()
        val processedText = try {
            ValdiProcessedText.parse(
                createFontManager(getApplicationContext()),
                FakeAttributedText(
                    listOf(
                        Part(
                            "chip",
                            inlineViewAttachment = inlineAttachment,
                            animationTransform = TextAnimationTransform(
                                key = "intro",
                                translationY = 4f,
                                scale = 0.5f,
                                opacity = 0f,
                                duration = 0.25,
                                timeOffsetBetweenParts = 0.0,
                                groupIndex = 0,
                                partIndexInGroup = 0,
                                partPattern = "."
                            )
                        )
                    )
                ),
                FontAttributes.default,
                NoopMissingFontsTracker(),
                NoopLogger,
                attributedTextAnimator = animator
            )
        } finally {
            animator.endSync()
        }

        assertTrue(processedText.hasAnimationTransform)
        assertEquals(1, processedText.animationTransformsCount)
        val inlineSpan = processedText.inlineViewAttachments!![0].value
        assertSame(processedText.animationTransforms!![0].value, inlineSpan.animation)
        val replacementSpans = processedText.spannable.getSpans(
            0,
            "chip".length,
            AnimatedTextReplacementSpan::class.java
        )
        assertEquals(0, replacementSpans.size)
    }

    @Test
    fun updateInlineAttachmentsReturnsFalseWhenSizesAreCurrent() {
        val inlinePart = Part(
            "chip",
            inlineViewAttachment = InlineViewAttachmentInfo(
                childIndex = 0,
                verticalAlignment = InlineViewVerticalAlignment.Center,
                width = 10f,
                height = 6f
            )
        )
        val processedText = ValdiProcessedText.parse(
            createFontManager(getApplicationContext()),
            FakeAttributedText(listOf(inlinePart)),
            FontAttributes.default,
            NoopMissingFontsTracker(),
            NoopLogger,
            density = 1f
        )

        assertFalse(processedText.updateInlineAttachments())
        val inlineViewAttachments = processedText.inlineViewAttachments!!
        assertEquals(10, inlineViewAttachments[0].value.layoutSize.width)
        assertEquals(6, inlineViewAttachments[0].value.layoutSize.height)
    }

    @Test
    fun updateInlineAttachmentsRefreshesSizeFromAttributedText() {
        val inlinePart = Part(
            "chip",
            inlineViewAttachment = InlineViewAttachmentInfo(
                childIndex = 0,
                verticalAlignment = InlineViewVerticalAlignment.Center,
                width = 10f,
                height = 6f
            )
        )
        val processedText = ValdiProcessedText.parse(
            createFontManager(getApplicationContext()),
            FakeAttributedText(listOf(inlinePart)),
            FontAttributes.default,
            NoopMissingFontsTracker(),
            NoopLogger,
            density = 2f
        )

        inlinePart.inlineViewAttachment = InlineViewAttachmentInfo(
            childIndex = 0,
            verticalAlignment = InlineViewVerticalAlignment.Center,
            width = 24f,
            height = 10f
        )

        assertTrue(processedText.updateInlineAttachments())
        val inlineViewAttachments = processedText.inlineViewAttachments!!
        assertEquals(48, inlineViewAttachments[0].value.layoutSize.width)
        assertEquals(20, inlineViewAttachments[0].value.layoutSize.height)
    }
}
