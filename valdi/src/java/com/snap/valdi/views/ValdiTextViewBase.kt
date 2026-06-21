package com.snap.valdi.views

import android.content.Context
import android.graphics.Canvas
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.InlineViewAttachmentSpan
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.extensions.removeFromParentView
import com.snap.valdi.extensions.ViewUtils
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Shared Android container for Valdi label, textview, and textfield variants.
 *
 * The native Android text control remains the backing [TextView], but Valdi now
 * exposes these elements as a ViewGroup so real Valdi children can be inserted
 * for inline-view attributed-text parts. Yoga computes child sizes in the normal
 * runtime, then this base class uses TextView layout results to apply child
 * frames according to text layout semantics.
 */
abstract class ValdiTextViewBase(
    context: Context,
    val backingTextView: TextView
) : ViewGroup(context), ValdiRecyclableView, ValdiTextHolder, CustomChildViewAppender, ValdiChildFrameManagingView {

    private var inlineChildrenContainer: ViewGroup? = null

    final override var textViewHelper: TextViewHelper? = null
        set(value) {
            field = value
            if (value != null) {
                configureTextViewHelper(value)
            }
        }

    override var onSelectionChangeFunction: ValdiFunction? = null

    init {
        clipChildren = false
        clipToPadding = false
        addView(backingTextView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
    }

    protected open fun configureTextViewHelper(helper: TextViewHelper) {}

    fun getOrCreateTextViewHelper(
        fontManager: FontManager,
        defaultAttributes: FontAttributes,
        valueAttributeId: Int
    ): TextViewHelper {
        var helper = textViewHelper
        if (helper == null) {
            helper = TextViewHelper(backingTextView, fontManager, defaultAttributes, valueAttributeId, this)
            textViewHelper = helper
        }
        return helper
    }

    override fun addValdiChildView(childView: View, viewIndex: Int) {
        val container = getOrCreateInlineChildrenContainer()
        childView.removeFromParentView()
        container.addView(childView, viewIndex.coerceIn(0, container.childCount))
    }

    private fun getOrCreateInlineChildrenContainer(): ViewGroup {
        var container = inlineChildrenContainer
        if (container == null) {
            container = InlineTextChildrenContainer(context)
            inlineChildrenContainer = container
            addView(container, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
        }
        return container
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        textViewHelper?.updateTextAnimationGroupRegistration()
    }

    override fun onDetachedFromWindow() {
        textViewHelper?.unregisterTextAnimationGroup()
        super.onDetachedFromWindow()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        textViewHelper?.onMeasure(widthMeasureSpec, heightMeasureSpec)
        backingTextView.measure(widthMeasureSpec, resolveBackingHeightMeasureSpec(heightMeasureSpec))

        val measuredWidth = resolveSize(backingTextView.measuredWidth, widthMeasureSpec)
        val measuredHeight = resolveSize(backingTextView.measuredHeight, heightMeasureSpec)
        setMeasuredDimension(measuredWidth, measuredHeight)

        val exactWidth = MeasureSpec.makeMeasureSpec(measuredWidth, MeasureSpec.EXACTLY)
        val exactHeight = MeasureSpec.makeMeasureSpec(measuredHeight, MeasureSpec.EXACTLY)
        backingTextView.measure(exactWidth, exactHeight)
        inlineChildrenContainer?.measure(exactWidth, exactHeight)
    }

    protected open fun resolveBackingHeightMeasureSpec(heightMeasureSpec: Int): Int {
        return heightMeasureSpec
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        val width = right - left
        val height = bottom - top
        val inlineAttachmentsDidChange = textViewHelper?.updateInlineAttachments() == true
        if (inlineAttachmentsDidChange) {
            textViewHelper?.refreshProcessedTextStorage()
            backingTextView.measure(
                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                resolveBackingHeightMeasureSpec(MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
            )
        }
        backingTextView.layout(0, 0, width, height)
        // TextViewHelper reads backing TextView bounds for effects such as
        // text gradients, so run it after the backing view has its committed
        // frame rather than while it still has the previous layout pass bounds.
        textViewHelper?.onLayout(changed || inlineAttachmentsDidChange)
        inlineChildrenContainer?.layout(0, 0, width, height)
        updateInlineTextChildFrames()
    }

    override fun dispatchDraw(canvas: Canvas) {
        textViewHelper?.postInvalidateOnAnimationIfNeeded()
        super.dispatchDraw(canvas)
    }

    override fun drawChild(canvas: Canvas, child: View, drawingTime: Long): Boolean {
        val didDraw = super.drawChild(canvas, child, drawingTime)
        if (child === backingTextView) {
            drawOnTopAttributedText(canvas)
        }
        return didDraw
    }

    private fun drawOnTopAttributedText(canvas: Canvas) {
        val helper = textViewHelper ?: return
        if (!helper.needsDrawOnTopAttributedText()) {
            return
        }
        val layout = backingTextView.layout ?: return
        canvas.save()
        canvas.translate(backingTextView.left.toFloat(), backingTextView.top.toFloat())
        helper.drawOnTopAttributedText(canvas, layout)
        canvas.restore()
    }

    private fun updateInlineTextChildFrames() {
        val container = inlineChildrenContainer ?: return

        val processedText = textViewHelper?.processedText

        val layout = backingTextView.layout

        for (childIndex in 0 until container.childCount) {
            val childView = container.getChildAt(childIndex)
            if (processedText == null) {
                clearInlineTextChildFrame(childView)
                continue
            }

            val item = processedText.inlineViewAttachmentForViewIndex(childIndex)
            if (item == null) {
                clearInlineTextChildFrame(childView)
                continue
            }

            if (layout == null) {
                continue
            }

            val span = item.value
            val spanStart = item.start
            val spanEnd = item.end
            if (spanStart < 0 || spanStart > spanEnd || spanEnd > processedText.spannable.length) {
                continue
            }

            val layoutSize = span.layoutSize
            val line = layout.getLineForOffset(spanStart)
            val startHorizontal = layout.getPrimaryHorizontal(spanStart)
            val endHorizontal = layout.getPrimaryHorizontal(spanEnd)
            val leftInText = if (spanEnd < layout.getLineEnd(line) && layout.getLineForOffset(spanEnd) == line) {
                min(startHorizontal, endHorizontal)
            } else {
                startHorizontal
            }
            val baseline = layout.getLineBaseline(line).toFloat()
            val childTop = InlineViewAttachmentSpan.resolveChildTop(
                span.fontMetrics ?: backingTextView.paint.fontMetrics,
                layoutSize.height,
                span.attachmentInfo.verticalAlignment
            )
            val topInText = baseline + childTop

            val left = backingTextView.left + backingTextView.totalPaddingLeft - backingTextView.scrollX + leftInText.roundToInt()
            val top = backingTextView.top + backingTextView.totalPaddingTop - backingTextView.scrollY + topInText.roundToInt()
            val right = left + layoutSize.width
            val bottom = top + layoutSize.height

            applyLayoutToChild(childView, left, top, right, bottom)
        }
    }

    private fun clearInlineTextChildFrame(childView: View) {
        applyLayoutToChild(childView, 0, 0, 0, 0)
    }

    private fun applyLayoutToChild(childView: View, left: Int, top: Int, right: Int, bottom: Int) {
        val width = right - left
        val height = bottom - top
        ViewUtils.setCalculatedFrame(childView, left, top, width, height)
        childView.measure(
            View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY)
        )
        childView.layout(left, top, right, bottom)
        ViewUtils.notifyDidApplyLayout(childView)
    }

    override fun setTextAccessibility(text: CharSequence?) {
        backingTextView.setText(text, null)
    }

    override fun setValdiSelectable(selectable: Boolean) {
        backingTextView.setTextIsSelectable(selectable)
    }

    override fun setValdiSelection(start: Int, end: Int) {
        ValdiTextSelection.setSelectionClamped(backingTextView, start, end)
    }

    override fun prepareForRecycling() {
        textViewHelper?.unregisterTextAnimationGroup()
    }

    /**
     * Lazily-created child host layered above the backing TextView.
     *
     * Its own layout is intentionally empty: ValdiTextViewBase applies exact
     * frames to each inline child after TextView has resolved the attachment
     * positions.
     */
    private class InlineTextChildrenContainer(context: Context) : ViewGroup(context) {
        init {
            clipChildren = false
            clipToPadding = false
        }

        override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
            setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec))
        }

        override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
            // Inline child frames are resolved by TextViewHelper from text layout.
        }
    }
}
