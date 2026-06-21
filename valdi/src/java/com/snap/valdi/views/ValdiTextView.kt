package com.snap.valdi.views

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Canvas
import android.text.TextDirectionHeuristic
import android.text.Spannable
import android.view.MotionEvent
import android.widget.TextView
import com.snap.valdi.attributes.impl.richtext.AttributedText
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.attributes.impl.richtext.hasActiveAnimationTransform
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.trace

class ValdiTextView(context: Context) : TextView(context), ValdiRecyclableView, ValdiTextHolder, ValdiTouchTarget {
    companion object {
        private const val IMAGE_ATTACHMENT_BREAK_CHAR = '\u2009'
    }

    override var textViewHelper: TextViewHelper? = null
    override var onSelectionChangeFunction: ValdiFunction? = null
    private var attributedText: AttributedText? = null
    private var cachedRenderedContent: String? = null
    private var cachedRenderedPartLengths: IntArray? = null
    private val shouldValidateRenderedContent =
        context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE != 0

    init {
        TextViewUtils.configure(this)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        textViewHelper?.updateTextAnimationGroupRegistration()
    }

    override fun onDetachedFromWindow() {
        textViewHelper?.unregisterTextAnimationGroup()
        super.onDetachedFromWindow()
    }

    override fun prepareForRecycling() {
        textViewHelper?.unregisterTextAnimationGroup()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        trace({"ValdiTextView.onMeasure"}) {
            textViewHelper?.onMeasure(widthMeasureSpec, heightMeasureSpec)
            super.onMeasure(widthMeasureSpec, TextViewUtils.resolveHeightMeasureSpec(this, heightMeasureSpec))
        }
    }

    override fun getTextDirectionHeuristic(): TextDirectionHeuristic {
        return TextViewUtils.resolveTextDirectionHeuristic(super.getTextDirectionHeuristic())
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        textViewHelper?.onLayout(changed)
        super.onLayout(changed, left, top, right, bottom)
    }

    override fun onDraw(canvas: Canvas) {
        val currentAttributedText = attributedText
        if (currentAttributedText != null && currentAttributedText.hasActiveAnimationTransform()) {
            val currentLayout = layout
            if (currentLayout != null) {
                if (textViewHelper?.drawOnTopAttributedText(canvas, currentLayout, currentAttributedText) == true) {
                    textViewHelper?.postInvalidateOnAnimationIfNeeded()
                    return
                }
            }
        }
        super.onDraw(canvas)
        textViewHelper?.postInvalidateOnAnimationIfNeeded()
    }

    fun setAttributedText(attributedText: AttributedText, spannable: Spannable) {
        this.attributedText = attributedText
        cachedRenderedContent = if (shouldValidateRenderedContent) {
            buildRenderedTextContent(attributedText)
        } else {
            null
        }
        cachedRenderedPartLengths = if (shouldValidateRenderedContent) {
            buildRenderedPartLengths(attributedText)
        } else {
            null
        }
        super.setText(spannable, BufferType.SPANNABLE)
    }

    fun updateAttributedText(attributedText: AttributedText): Boolean {
        if (shouldValidateRenderedContent && !hasSameRenderedTextContent(attributedText)) {
            return false
        }
        this.attributedText = attributedText
        invalidate()
        return true
    }

    fun clearAttributedText() {
        attributedText = null
        cachedRenderedContent = null
        cachedRenderedPartLengths = null
        textViewHelper?.clearOverlayLayoutCache()
        invalidate()
    }

    override fun setTextAccessibility(text: CharSequence?) {
        super.setText(text, null)
    }

    override fun prepareForRecycling() {
        textViewHelper?.clearOverlayLayoutCache()
    }

    private fun buildRenderedTextContent(attributedText: AttributedText): String {
        val renderedText = StringBuilder()
        for (index in 0 until attributedText.getPartsSize()) {
            renderedText.append(attributedText.getContentAtIndex(index))
            if (attributedText.getImageAttachmentAtIndex(index) != null) {
                renderedText.append(IMAGE_ATTACHMENT_BREAK_CHAR)
            }
        }
        return renderedText.toString()
    }

    private fun buildRenderedPartLengths(attributedText: AttributedText): IntArray {
        return IntArray(attributedText.getPartsSize()) { index ->
            attributedText.getContentAtIndex(index).length
        }
    }

    private fun hasSameRenderedTextContent(attributedText: AttributedText): Boolean {
        val cachedContent = cachedRenderedContent ?: return false
        val cachedPartLengths = cachedRenderedPartLengths ?: return false
        if (cachedPartLengths.size != attributedText.getPartsSize()) {
            return false
        }
        var offset = 0
        for (index in 0 until attributedText.getPartsSize()) {
            val content = attributedText.getContentAtIndex(index)
            if (cachedPartLengths[index] != content.length) {
                return false
            }
            if (!cachedContent.regionMatches(offset, content, 0, content.length)) {
                return false
            }
            offset += content.length
            if (attributedText.getImageAttachmentAtIndex(index) != null) {
                if (offset >= cachedContent.length || cachedContent[offset] != IMAGE_ATTACHMENT_BREAK_CHAR) {
                    return false
                }
                offset++
            }
        }
        return offset == cachedContent.length
    }

    override fun setValdiSelectable(selectable: Boolean) {
        setTextIsSelectable(selectable)
    }

    override fun setValdiSelection(start: Int, end: Int) {
        ValdiTextSelection.setSelectionClamped(this, start, end)
    }

    override fun onSelectionChanged(selStart: Int, selEnd: Int) {
        super.onSelectionChanged(selStart, selEnd)
        ValdiTextSelection.notifySelectionChanged(this, selStart, selEnd)
        ValdiTextSelection.callSelectionChangeCallback(onSelectionChangeFunction, text, selStart, selEnd)
    }

    override fun allowsSameViewGestureRecognizers(): Boolean = true

    override fun processTouchEvent(event: MotionEvent): ValdiTouchEventResult {
        if (!isTextSelectable) {
            return ValdiTouchEventResult.IgnoreEvent
        }

        return if (dispatchTouchEvent(event)) {
            ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        } else {
            ValdiTouchEventResult.IgnoreEvent
        }
    }
}
