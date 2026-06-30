package com.snap.valdi.views

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import com.snap.valdi.attributes.impl.richtext.AttributedText
import com.snap.valdi.attributes.impl.richtext.AttributedTextAnimation
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.ImageAttachmentInfo
import com.snap.valdi.attributes.impl.richtext.InlineViewAttachmentInfo
import com.snap.valdi.attributes.impl.richtext.InlineViewVerticalAlignment
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.attributes.impl.richtext.TextAnimationTransform
import com.snap.valdi.attributes.impl.richtext.TextDecoration
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.Logger
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class ValdiTextViewTest {
    private object NoopLogger : Logger {
        override fun log(level: Int, message: String?) = Unit
        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    private class Part(
        val content: String,
        val inlineViewAttachment: InlineViewAttachmentInfo? = null,
        val animationTransform: TextAnimationTransform? = null
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
        override fun getOutlineColorAtIndex(index: Int): Int? = null
        override fun getOutlineWidthAtIndex(index: Int): Float = 0f
        override fun hasOutline(): Boolean = false
        override fun getAnimationTransformsSize(): Int = parts.count { it.animationTransform != null }
        override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? = null
        override fun getInlineViewAttachmentAtIndex(index: Int): InlineViewAttachmentInfo? = parts[index].inlineViewAttachment
        override fun getAnimationTransformAtIndex(index: Int): TextAnimationTransform? = parts[index].animationTransform
    }

    private class TestValdiTextViewBase(context: Context) : ValdiTextViewBase(context, TextView(context)) {
        data class InlineAnimationUpdate(
            val childView: View,
            val animation: AttributedTextAnimation?
        )

        val inlineAnimationUpdates = mutableListOf<InlineAnimationUpdate>()

        init {
            TextViewUtils.configure(backingTextView)
        }

        override fun applyInlineTextAnimationAttributes(childView: View, animation: AttributedTextAnimation?) {
            inlineAnimationUpdates.add(InlineAnimationUpdate(childView, animation))
        }
    }

    private fun createFontManager(context: Context): FontManager {
        return FontManager(context, object : TypefaceResLoader {
            override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
        })
    }

    @Test
    fun labelIsAViewGroupBackedByATextView() {
        val textView = ValdiTextView(getApplicationContext<Context>())

        assertSame(ViewGroup::class.java, ValdiTextViewBase::class.java.superclass)
        assertEquals(1, textView.childCount)
        assertSame(textView.backingTextView, textView.getChildAt(0))
    }

    @Test
    fun labelAddsValdiChildrenIntoInlineContainer() {
        val context = getApplicationContext<Context>()
        val textView = ValdiTextView(context)
        val firstChild = View(context)
        val secondChild = View(context)

        textView.addValdiChildView(firstChild, 0)

        assertEquals(2, textView.childCount)
        val inlineContainer = firstChild.parent as ViewGroup
        assertSame(textView.getChildAt(1), inlineContainer)
        assertSame(firstChild, inlineContainer.getChildAt(0))

        textView.addValdiChildView(secondChild, 0)

        assertEquals(2, inlineContainer.childCount)
        assertSame(secondChild, inlineContainer.getChildAt(0))
        assertSame(firstChild, inlineContainer.getChildAt(1))
    }

    @Test
    fun labelInlineChildLayoutKeepsChildIndexMappingStable() {
        val context = getApplicationContext<Context>()
        val textView = TestValdiTextViewBase(context)
        val children = List(4) { View(context) }

        val attributedText = FakeAttributedText(
            listOf(
                Part("A "),
                Part("\uFFFC", InlineViewAttachmentInfo(0, InlineViewVerticalAlignment.Top, 8f, 6f)),
                Part(" B "),
                Part("\uFFFC", InlineViewAttachmentInfo(1, InlineViewVerticalAlignment.Center, 8f, 6f)),
                Part(" C "),
                Part("\uFFFC", InlineViewAttachmentInfo(2, InlineViewVerticalAlignment.Bottom, 8f, 6f)),
                Part(" D "),
                Part("\uFFFC", InlineViewAttachmentInfo(3, InlineViewVerticalAlignment.Baseline, 8f, 6f)),
                Part(" E")
            )
        )
        textView.getOrCreateTextViewHelper(
            createFontManager(context),
            FontAttributes.default.copy(fontSize = 20f, numberOfLines = 0),
            0,
            NoopLogger
        ).textValue = attributedText
        children.forEachIndexed { index, child ->
            textView.addValdiChildView(child, index)
        }

        val widthSpec = View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(160, View.MeasureSpec.EXACTLY)
        textView.measure(widthSpec, heightSpec)
        textView.layout(0, 0, textView.measuredWidth, textView.measuredHeight)

        children.forEach { child ->
            assertTrue("Expected every inline child to be laid out", child.width > 0)
            assertTrue("Expected every inline child to be laid out", child.height > 0)
        }
        assertTrue(children[0].left < children[1].left)
        assertTrue(children[1].left < children[2].left)
        assertTrue(children[2].left < children[3].left)

        textView.textViewHelper?.textValue = FakeAttributedText(listOf(Part("plain text")))
        textView.measure(widthSpec, heightSpec)
        textView.layout(0, 0, textView.measuredWidth, textView.measuredHeight)

        children.forEach { child ->
            assertEquals(0, child.width)
            assertEquals(0, child.height)
        }
    }

    @Test
    fun labelInlineChildLayoutUsesVisualLeftEdgeInRtlText() {
        val context = getApplicationContext<Context>()
        val textView = TestValdiTextViewBase(context)
        val child = View(context)

        textView.backingTextView.layoutDirection = View.LAYOUT_DIRECTION_RTL
        textView.getOrCreateTextViewHelper(
            createFontManager(context),
            FontAttributes.default.copy(fontSize = 20f, numberOfLines = 0),
            0,
            NoopLogger
        ).textValue = FakeAttributedText(
            listOf(
                Part("אבג "),
                Part("\uFFFC", InlineViewAttachmentInfo(0, InlineViewVerticalAlignment.Center, 40f, 12f))
            )
        )
        textView.addValdiChildView(child, 0)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(240, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(80, View.MeasureSpec.EXACTLY)
        textView.measure(widthSpec, heightSpec)
        textView.layout(0, 0, textView.measuredWidth, textView.measuredHeight)

        assertTrue("Expected the RTL inline child to be laid out", child.width > 0)
        assertTrue(
            "Expected RTL inline child to stay inside the label instead of starting at the right caret edge",
            child.right <= textView.width
        )
    }

    @Test
    fun labelInlineChildLayoutAppliesAnimatedAttributesAndResetsThem() {
        val context = getApplicationContext<Context>()
        val textView = TestValdiTextViewBase(context)
        val child = View(context)
        val animationTransform = TextAnimationTransform(
            key = "inline-alpha",
            translationY = 8f,
            scale = 0.7f,
            opacity = 0f,
            duration = 100.0,
            timeOffsetBetweenParts = 0.0,
            groupIndex = 0,
            partIndexInGroup = 0,
            partPattern = null
        )

        textView.getOrCreateTextViewHelper(
            createFontManager(context),
            FontAttributes.default.copy(fontSize = 20f, numberOfLines = 0),
            0,
            NoopLogger
        ).textValue = FakeAttributedText(
            listOf(
                Part("Before "),
                Part(
                    "\uFFFC",
                    InlineViewAttachmentInfo(0, InlineViewVerticalAlignment.Center, 40f, 12f),
                    animationTransform
                ),
                Part(" after")
            )
        )
        textView.addValdiChildView(child, 0)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(320, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(80, View.MeasureSpec.EXACTLY)
        textView.measure(widthSpec, heightSpec)
        textView.layout(0, 0, textView.measuredWidth, textView.measuredHeight)

        val animatedUpdate = textView.inlineAnimationUpdates.last {
            it.childView === child && it.animation != null
        }
        val animation = animatedUpdate.animation!!
        assertTrue("Expected animated inline child opacity to be below fully visible", animation.opacity < 1f)
        assertTrue("Expected animated inline child translation to be active", animation.translationY > 0f)
        assertTrue("Expected animated inline child scale to be active", animation.scale < 1f)

        textView.textViewHelper?.textValue = FakeAttributedText(listOf(Part("plain text")))
        textView.measure(widthSpec, heightSpec)
        textView.layout(0, 0, textView.measuredWidth, textView.measuredHeight)

        val resetUpdate = textView.inlineAnimationUpdates.last { it.childView === child }
        assertNull(resetUpdate.animation)
        assertEquals(0, child.width)
        assertEquals(0, child.height)
    }

    @Test
    fun textAnimationPartCountUsesExpandedPartPatternRanges() {
        val context = getApplicationContext<Context>()
        val textView = TestValdiTextViewBase(context)
        val helper = textView.getOrCreateTextViewHelper(
            createFontManager(context),
            FontAttributes.default.copy(fontSize = 20f, numberOfLines = 0),
            0,
            NoopLogger
        )

        helper.textValue = FakeAttributedText(
            listOf(
                Part(
                    "abc",
                    animationTransform = TextAnimationTransform(
                        key = "chars",
                        translationY = 8f,
                        scale = 0.7f,
                        opacity = 0f,
                        duration = 100.0,
                        timeOffsetBetweenParts = 0.1,
                        groupIndex = 0,
                        partIndexInGroup = 0,
                        partPattern = "."
                    )
                )
            )
        )

        val widthSpec = View.MeasureSpec.makeMeasureSpec(320, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(80, View.MeasureSpec.EXACTLY)
        textView.measure(widthSpec, heightSpec)

        assertEquals(3, helper.textAnimationPartCount)
    }

    @Test
    fun labelTextGradientInitializesFromFinalBackingTextViewBounds() {
        val context = getApplicationContext<Context>()
        val textView = TestValdiTextViewBase(context)
        val helper = textView.getOrCreateTextViewHelper(
            createFontManager(context),
            FontAttributes.default.copy(fontSize = 20f),
            0,
            NoopLogger
        )
        helper.textValue = "Gradient text"
        helper.textGradient = ValdiGradient(
            intArrayOf(Color.RED, Color.BLUE),
            null,
            false,
            0
        )

        val widthSpec = View.MeasureSpec.makeMeasureSpec(320, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(72, View.MeasureSpec.EXACTLY)
        textView.measure(widthSpec, heightSpec)
        textView.layout(0, 0, textView.measuredWidth, textView.measuredHeight)

        assertNotNull(textView.backingTextView.paint.shader)
        val initialGradientSizeField = TextViewHelper::class.java.getDeclaredField("initialGradientSize")
        initialGradientSizeField.isAccessible = true
        val initialGradientSize = initialGradientSizeField.get(helper) as android.util.Size
        assertEquals(textView.backingTextView.width, initialGradientSize.width)
        assertEquals(textView.backingTextView.height, initialGradientSize.height)
    }

    @Test
    fun labelSelectionIsDisabledByDefault() {
        val textView = ValdiTextView(getApplicationContext<Context>())
        textView.text = "Selectable label text"

        assertFalse(textView.isTextSelectable)
    }

    @Test
    fun labelSelectableTogglesNativeTextSelection() {
        val textView = ValdiTextView(getApplicationContext<Context>())
        textView.text = "Selectable label text"

        textView.setValdiSelectable(true)
        assertTrue(textView.isTextSelectable)

        textView.setValdiSelectable(false)
        assertFalse(textView.isTextSelectable)
    }
}
