package com.snap.valdi.attributes.impl.richtext

import android.graphics.Canvas
import android.graphics.Paint
import android.text.style.ReplacementSpan
import kotlin.math.roundToInt

/**
 * Pixel size reserved in Android text layout for an inline Valdi child view.
 */
class InlineViewAttachmentSize(
    val width: Int,
    val height: Int
)

/**
 * ReplacementSpan that reserves text-layout space for an inline Valdi child.
 *
 * The span does not draw anything itself. It participates in TextView
 * measurement, captures the font metrics used for the line, and exposes enough
 * information for ValdiTextViewBase to position the actual child View after
 * Android has resolved bidi ordering, wrapping, and line metrics.
 */
class InlineViewAttachmentSpan(
    attachmentInfo: InlineViewAttachmentInfo,
    private val density: Float,
    val animation: AttributedTextAnimation?
) : ReplacementSpan() {

    var attachmentInfo: InlineViewAttachmentInfo = attachmentInfo
        private set

    private var measuredLayoutSize = resolveLayoutSize()
    private var measuredFontMetrics: Paint.FontMetrics? = null

    val layoutSize: InlineViewAttachmentSize
        get() = measuredLayoutSize

    val fontMetrics: Paint.FontMetrics?
        get() = measuredFontMetrics

    fun updateLayoutSize(): Boolean {
        val nextLayoutSize = resolveLayoutSize()
        if (nextLayoutSize.width == measuredLayoutSize.width && nextLayoutSize.height == measuredLayoutSize.height) {
            return false
        }
        measuredLayoutSize = nextLayoutSize
        return true
    }

    fun updateAttachmentInfo(nextAttachmentInfo: InlineViewAttachmentInfo): Boolean {
        attachmentInfo = nextAttachmentInfo
        return updateLayoutSize()
    }

    private fun resolveLayoutSize(): InlineViewAttachmentSize {
        return InlineViewAttachmentSize(
            (attachmentInfo.width * density).roundToInt(),
            (attachmentInfo.height * density).roundToInt()
        )
    }

    override fun getSize(paint: Paint, text: CharSequence?, start: Int, end: Int, fm: Paint.FontMetricsInt?): Int {
        val size = layoutSize
        if (fm != null) {
            val fontMetrics = paint.fontMetrics
            measuredFontMetrics = fontMetrics.copy()
            val childTop = resolveChildTop(fontMetrics, size.height, attachmentInfo.verticalAlignment)
            val childBottom = childTop + size.height
            if (childTop < fm.ascent) fm.ascent = childTop
            if (childTop < fm.top) fm.top = childTop
            if (childBottom > fm.descent) fm.descent = childBottom
            if (childBottom > fm.bottom) fm.bottom = childBottom
        }
        return size.width
    }

    override fun draw(canvas: Canvas, text: CharSequence?, start: Int, end: Int, x: Float, top: Int, y: Int, bottom: Int, paint: Paint) {
        // The actual child view is laid out by TextViewHelper after TextView layout resolves this span.
    }

    companion object {
        fun resolveChildTop(
            fontMetrics: Paint.FontMetrics,
            childHeight: Int,
            verticalAlignment: InlineViewVerticalAlignment
        ): Int {
            return when (verticalAlignment) {
                InlineViewVerticalAlignment.Top -> fontMetrics.ascent.toInt()
                InlineViewVerticalAlignment.Center ->
                    ((fontMetrics.descent + fontMetrics.ascent - childHeight) / 2.0f).toInt()
                InlineViewVerticalAlignment.Bottom -> (fontMetrics.descent - childHeight).toInt()
                InlineViewVerticalAlignment.Baseline -> -childHeight
            }
        }
    }
}

private fun Paint.FontMetrics.copy(): Paint.FontMetrics {
    val copy = Paint.FontMetrics()
    copy.ascent = ascent
    copy.descent = descent
    copy.top = top
    copy.bottom = bottom
    copy.leading = leading
    return copy
}
