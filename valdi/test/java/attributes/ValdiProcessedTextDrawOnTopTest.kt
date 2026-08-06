package com.snap.valdi.attributes

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.attributes.impl.richtext.AttributedText
import com.snap.valdi.attributes.impl.richtext.AttributedTextAnimator
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.ImageAttachmentInfo
import com.snap.valdi.attributes.impl.richtext.InlineViewAttachmentInfo
import com.snap.valdi.attributes.impl.richtext.TextAnimationTransform
import com.snap.valdi.attributes.impl.richtext.TextDecoration
import com.snap.valdi.attributes.impl.richtext.ValdiProcessedText
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.CoordinateResolver
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode

/**
 * Covers [ValdiProcessedText.drawOnTop] and the range guards in
 * [ValdiProcessedText.updateOnLayoutCallbacks].
 *
 * These are the new-architecture homes for behavior that PR #107 removed coverage for when it cut
 * AnimationRichTextTest from 34 cases to 7. The old cases asserted against an overlay layout cache
 * and a resolveLayoutOrigin/chunkOrigin geometry helper, neither of which exists any more, so they
 * could not be restored -- the concerns moved into drawOnTop, which walks the Layout directly.
 * Specifically this replaces:
 *
 *   - overlayRenderModeKeepsStaticFillVisible,
 *     overlayRenderModeKeepsInactiveAnimatedFillVisibleInCache and
 *     baseRenderModeUsesTransparentStyleForAnimatedTextWithoutOutline
 *       -> drawOnTop draws an outlined static range and skips an animated one.
 *   - resolveLayoutOriginDoesNotDoubleApplyScrollOffsets and
 *     resolveLayoutOriginDoesNotApplyHorizontalGravityOffset
 *       -> drawOnTop takes coordinates straight from the Layout, with nothing added.
 *   - rtlChunkBitmapLeftUsesChunkOriginWithoutDoubleApplyingLineLeft and
 *     ltrRunInRtlParagraphKeepsChunkOriginAtPrimaryHorizontal
 *       -> the RTL case asserts the same primary-horizontal contract.
 *
 * NATIVE graphics is required: text must actually shape and wrap for line splitting and
 * primary-horizontal positions to be meaningful.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
internal class ValdiProcessedTextDrawOnTopTest {

    private lateinit var fontManager: FontManager
    private lateinit var missingFontsTracker: MissingFontsTracker

    private object NoopLogger : Logger {
        override fun log(level: Int, message: String?) = Unit
        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    private data class DrawnText(val text: String, val x: Float, val y: Float)

    /** Records the drawText calls drawOnTop makes, so placement can be asserted exactly. */
    private class RecordingCanvas(bitmap: Bitmap) : Canvas(bitmap) {
        val drawn = mutableListOf<DrawnText>()

        override fun drawText(text: String, x: Float, y: Float, paint: Paint) {
            drawn.add(DrawnText(text, x, y))
            super.drawText(text, x, y, paint)
        }
    }

    private class Part(
        val content: String,
        val outlineColor: Int? = null,
        val outlineWidth: Float = 0f,
        val animationTransform: TextAnimationTransform? = null,
    )

    private class FakeAttributedText(private val parts: List<Part>) : AttributedText {
        override fun getPartsSize(): Int = parts.size
        override fun getContentAtIndex(index: Int): String = parts[index].content
        override fun getFontAtIndex(index: Int): String? = null
        override fun getTextDecorationAtIndex(index: Int): TextDecoration? = null
        override fun getColorAtIndex(index: Int): Int? = null
        override fun getBackgroundColorAtIndex(index: Int): Int? = null
        override fun getOnTapAtIndex(index: Int): ValdiFunction? = null
        override fun getOnLayoutAtIndex(index: Int): ValdiFunction? = null
        override fun getOutlineColorAtIndex(index: Int): Int? = parts[index].outlineColor
        override fun getOutlineWidthAtIndex(index: Int): Float = parts[index].outlineWidth
        override fun hasOutline(): Boolean = parts.any { it.outlineColor != null && it.outlineWidth > 0f }
        override fun getAnimationTransformsSize(): Int = parts.count { it.animationTransform != null }
        override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? = null
        override fun getInlineViewAttachmentAtIndex(index: Int): InlineViewAttachmentInfo? = null
        override fun getAnimationTransformAtIndex(index: Int): TextAnimationTransform? =
            parts[index].animationTransform
    }

    @Before
    fun setUp() {
        // Required for the RTL case: without these the paragraph resolves LTR and Hebrew text lays
        // out at x=0, which would let outlinedRtlTextUsesThePrimaryHorizontalOfItsLine pass while
        // proving nothing about RTL.
        val appInfo = getApplicationContext<Context>().applicationInfo
        appInfo.targetSdkVersion = 28
        appInfo.flags = appInfo.flags or android.content.pm.ApplicationInfo.FLAG_SUPPORTS_RTL

        fontManager = FontManager(getApplicationContext(), object : TypefaceResLoader {
            override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
        })
        missingFontsTracker = object : MissingFontsTracker {
            override fun onFontMissing(fontDescriptor: FontDescriptor) = Unit
        }
    }

    private fun parse(
        text: FakeAttributedText,
        animated: Boolean = false,
        disableTextReplacement: Boolean = false,
    ): ValdiProcessedText {
        if (!animated) {
            return ValdiProcessedText.parse(
                fontManager,
                text,
                FontAttributes.default,
                missingFontsTracker,
                NoopLogger,
                disableTextReplacement = disableTextReplacement,
            )
        }
        val animator = AttributedTextAnimator()
        animator.beginSync()
        return try {
            ValdiProcessedText.parse(
                fontManager,
                text,
                FontAttributes.default,
                missingFontsTracker,
                NoopLogger,
                animator,
                disableTextReplacement = disableTextReplacement,
            )
        } finally {
            animator.endSync()
        }
    }

    private fun layoutOf(processed: ValdiProcessedText, width: Int): Layout {
        return StaticLayout.Builder
            .obtain(processed.spannable, 0, processed.spannable.length, TextPaint(Paint.ANTI_ALIAS_FLAG), width)
            .setAlignment(Layout.Alignment.ALIGN_NORMAL)
            .setIncludePad(false)
            .build()
    }

    private fun drawAndRecord(processed: ValdiProcessedText, layout: Layout): List<DrawnText> {
        val bitmap = Bitmap.createBitmap(400, 400, Bitmap.Config.ARGB_8888)
        val canvas = RecordingCanvas(bitmap)
        processed.drawOnTop(canvas, layout, fontManager, missingFontsTracker)
        return canvas.drawn
    }

    private fun outlined(content: String) =
        Part(content, outlineColor = Color.BLACK, outlineWidth = 2f)

    @Test
    fun outlinedStaticTextIsMarkedForDrawOnTopAndDrawnOnce() {
        val processed = parse(FakeAttributedText(listOf(outlined("Outlined"))))

        assertTrue("an outlined part should produce drawOnTop items", processed.hasOuterOutline)

        val drawn = drawAndRecord(processed, layoutOf(processed, 400))

        assertEquals(1, drawn.size)
        assertEquals("Outlined", drawn[0].text)
    }

    @Test
    fun textWithoutAnOutlineIsNeverDrawnOnTop() {
        val processed = parse(FakeAttributedText(listOf(Part("Plain"))))

        assertFalse("a part with no outline should produce no drawOnTop items", processed.hasOuterOutline)
        assertEquals(0, drawAndRecord(processed, layoutOf(processed, 400)).size)
    }

    @Test
    fun zeroWidthOutlineIsNotDrawnOnTop() {
        // outlineWidth must be strictly positive; a color alone is not enough.
        val processed = parse(
            FakeAttributedText(listOf(Part("Hairline", outlineColor = Color.BLACK, outlineWidth = 0f)))
        )

        assertFalse(processed.hasOuterOutline)
        assertEquals(0, drawAndRecord(processed, layoutOf(processed, 400)).size)
    }

    @Test
    fun drawnChunkTakesItsPositionStraightFromTheLayout() {
        val processed = parse(FakeAttributedText(listOf(outlined("Positioned"))))
        val layout = layoutOf(processed, 400)

        val drawn = drawAndRecord(processed, layout)

        // No scroll offset, no gravity offset, no line-left added on top: exactly the Layout's own
        // primary horizontal and baseline. This is the invariant the deleted resolveLayoutOrigin
        // tests were protecting.
        assertEquals(1, drawn.size)
        assertEquals(layout.getPrimaryHorizontal(0), drawn[0].x, 0.01f)
        assertEquals(layout.getLineBaseline(0).toFloat(), drawn[0].y, 0.01f)
    }

    @Test
    fun wrappedOutlinedTextIsDrawnOncePerLineWithNoCharactersLostOrRepeated() {
        val content = "outlined text long enough that it has to wrap onto several lines"
        // disableTextReplacement is required here, and is the configuration that makes the
        // multi-line branch of drawOnTop reachable at all. With replacement enabled the outlined
        // range becomes a single OutlineReplacementSpan, which StaticLayout treats as atomic and
        // will not break across lines, so the chunk walk never iterates. TextViewHelper disables
        // replacement for EditText precisely so the outline is drawn on top instead -- which is the
        // real caller of this loop.
        val processed = parse(
            FakeAttributedText(listOf(outlined(content))),
            disableTextReplacement = true,
        )
        // Narrow enough to force wrapping.
        val layout = layoutOf(processed, 120)
        assertTrue("expected the text to wrap", layout.lineCount > 1)

        val drawn = drawAndRecord(processed, layout)

        assertEquals("one draw call per line", layout.lineCount, drawn.size)
        // The chunk walk must partition the range exactly: concatenating the drawn pieces
        // reproduces the laid-out text.
        assertEquals(content, drawn.joinToString("") { it.text })
        // Each piece sits on its own line's baseline, in order.
        drawn.forEachIndexed { line, piece ->
            assertEquals(
                "line $line baseline",
                layout.getLineBaseline(line).toFloat(),
                piece.y,
                0.01f,
            )
        }
    }

    @Test
    fun animatedOutlinedRangeIsLeftToTheAnimatorRatherThanDrawnOnTop() {
        val animatedOutlined = Part(
            "Animated",
            outlineColor = Color.BLACK,
            outlineWidth = 2f,
            animationTransform = TextAnimationTransform(
                key = "intro",
                translationY = 0f,
                scale = 1.1f,
                opacity = 1f,
                duration = 0.25,
                timeOffsetBetweenParts = 0.0,
                groupIndex = 0,
                partIndexInGroup = 0,
                partPattern = null,
            ),
        )
        val processed = parse(FakeAttributedText(listOf(animatedOutlined)), animated = true)

        // Asserted on the parse result, not on the draw count. A fully animated outlined part
        // contributes no drawOnTop range at all -- addUnanimatedOutlineRanges only emits ranges for
        // the *unanimated* remainder, and here there is none -- so the animated replacement span
        // owns the outline exclusively.
        //
        // Deliberately NOT asserting `drawn.size == 0`: that passes whether or not drawOnTop honors
        // its `animation == null` condition, because there are no items to iterate. Verified by
        // mutation -- removing that condition fails nothing. In fact DrawOnTopValue is constructed
        // in exactly one place, always with animation = null, so the condition is unreachable and
        // the field is vestigial.
        assertFalse(
            "a fully animated outlined part should contribute no drawOnTop range",
            processed.hasOuterOutline,
        )
        assertEquals(0, drawAndRecord(processed, layoutOf(processed, 400)).size)
    }

    @Test
    fun outlinedRtlTextUsesThePrimaryHorizontalOfItsLine() {
        // Hebrew: the paragraph resolves RTL, so the run starts at the right edge. The contract is
        // still "whatever the Layout reports", with no mirroring applied by drawOnTop.
        val processed = parse(FakeAttributedText(listOf(outlined("שלום"))))
        val layout = layoutOf(processed, 400)

        val drawn = drawAndRecord(processed, layout)

        assertEquals(1, drawn.size)
        assertEquals(layout.getPrimaryHorizontal(0), drawn[0].x, 0.01f)
        assertEquals(layout.getLineBaseline(0).toFloat(), drawn[0].y, 0.01f)
    }

    @Test
    fun mixedPartsOnlyDrawTheOutlinedOne() {
        val processed = parse(
            FakeAttributedText(listOf(Part("plain "), outlined("outlined"), Part(" plain")))
        )

        val drawn = drawAndRecord(processed, layoutOf(processed, 400))

        assertEquals(1, drawn.size)
        assertEquals("outlined", drawn[0].text)
    }

    @Test
    fun updateOnLayoutCallbacksIsInertWithoutCallbacks() {
        // Named for what it actually covers. It does NOT reach the guard that skips ranges past the
        // end of the spannable: with no onLayout callbacks supplied, `onLayoutCallbacks ?: return`
        // returns first. Exercising that guard needs callbacks, which needs a native
        // ValdiMarshaller, so it stays uncovered rather than being implied by a test name.
        val processed = parse(FakeAttributedText(listOf(outlined("short"))))
        val layout = layoutOf(processed, 400)

        assertFalse(processed.hasOnLayout)
        processed.updateOnLayoutCallbacks(layout, CoordinateResolver(getApplicationContext()))
    }
}
