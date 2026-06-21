package com.snap.valdi.attributes.impl.richtext

import android.content.res.Resources
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.PathEffect
import android.text.Spanned
import android.text.TextPaint
import android.text.style.LineBackgroundSpan
import android.text.style.MetricAffectingSpan
import kotlin.math.roundToInt

abstract class PatternUnderlineSpan(private val animation: AttributedTextAnimation?) : LineBackgroundSpan {
    final override fun drawBackground(
        canvas: Canvas,
        paint: Paint,
        left: Int,
        right: Int,
        top: Int,
        baseline: Int,
        bottom: Int,
        text: CharSequence,
        start: Int,
        end: Int,
        lineNumber: Int
    ) {
        val spanned = text as? Spanned ?: return
        val spanStart = spanned.getSpanStart(this).coerceAtLeast(start)
        val spanEnd = spanned.getSpanEnd(this).coerceAtMost(end)
        if (spanStart >= spanEnd) {
            return
        }

        val startX = left + measureText(text, start, spanStart, paint)
        val endX = left + measureText(text, start, spanEnd, paint)
        val density = resolveDensity(paint)
        val strokeWidth = resolveStrokeWidth(paint, density)
        val resolvedStartX = resolveStartX(startX, strokeWidth, density)
        val resolvedEndX = resolveEndX(endX, strokeWidth, density)
        drawResolvedUnderline(canvas, paint, resolvedStartX, resolvedEndX, top, baseline, bottom, density, strokeWidth)
    }

    fun drawUnderlineRange(
        canvas: Canvas,
        paint: Paint,
        startX: Float,
        endX: Float,
        top: Int,
        baseline: Int,
        bottom: Int
    ) {
        val density = resolveDensity(paint)
        val strokeWidth = resolveStrokeWidth(paint, density)
        val resolvedStartX = resolveStartX(startX, strokeWidth, density)
        val resolvedEndX = resolveEndX(endX, strokeWidth, density)
        drawResolvedUnderline(canvas, paint, resolvedStartX, resolvedEndX, top, baseline, bottom, density, strokeWidth)
    }

    private fun drawResolvedUnderline(
        canvas: Canvas,
        paint: Paint,
        resolvedStartX: Float,
        resolvedEndX: Float,
        top: Int,
        baseline: Int,
        bottom: Int,
        density: Float,
        strokeWidth: Float
    ) {
        if (resolvedStartX > resolvedEndX) {
            return
        }

        val previousAlpha = paint.alpha
        if (animation != null) {
            val alpha = (animation.opacity * previousAlpha).roundToInt().coerceIn(0, 255)
            if (alpha <= 0) {
                return
            }
            paint.alpha = alpha
        }

        val underlineY = resolveVisibleUnderlineY(paint, baseline, bottom, strokeWidth, density)
        val previousPathEffect = paint.pathEffect
        val previousStyle = paint.style
        val previousStrokeWidth = paint.strokeWidth

        paint.style = resolvePaintStyle()
        paint.strokeWidth = strokeWidth
        paint.pathEffect = createPathEffect(strokeWidth, density)
        try {
            if (animation != null) {
                val centerX = (resolvedStartX + resolvedEndX) / 2f
                val centerY = (top + bottom) / 2f
                val saveCount = canvas.save()
                canvas.translate(centerX, centerY + animation.translationY * density)
                canvas.scale(animation.scale, animation.scale)
                canvas.translate(-centerX, -centerY)
                try {
                    canvas.drawLine(resolvedStartX, underlineY, resolvedEndX, underlineY, paint)
                } finally {
                    canvas.restoreToCount(saveCount)
                }
            } else {
                canvas.drawLine(resolvedStartX, underlineY, resolvedEndX, underlineY, paint)
            }
        } finally {
            paint.alpha = previousAlpha
            paint.pathEffect = previousPathEffect
            paint.style = previousStyle
            paint.strokeWidth = previousStrokeWidth
        }
    }

    protected open fun resolveStrokeWidth(paint: Paint, density: Float): Float {
        return paint.strokeWidth.coerceAtLeast(1f)
    }

    protected open fun resolveStartX(startX: Float, strokeWidth: Float, density: Float): Float {
        return startX
    }

    protected open fun resolveEndX(endX: Float, strokeWidth: Float, density: Float): Float {
        return endX
    }

    protected open fun resolveUnderlineY(
        paint: Paint,
        baseline: Int,
        strokeWidth: Float,
        density: Float
    ): Float {
        return baseline + paint.fontMetrics.descent * 0.5f
    }

    protected open fun resolveVisibleUnderlineY(
        paint: Paint,
        baseline: Int,
        bottom: Int,
        strokeWidth: Float,
        density: Float
    ): Float {
        return resolveUnderlineY(paint, baseline, strokeWidth, density)
            .coerceAtMost(bottom - strokeWidth / 2f)
    }

    protected open fun resolvePaintStyle(): Paint.Style {
        return Paint.Style.STROKE
    }

    protected abstract fun createPathEffect(strokeWidth: Float, density: Float): PathEffect?

    private fun measureText(
        text: CharSequence,
        start: Int,
        end: Int,
        paint: Paint
    ): Float {
        if (start >= end) {
            return 0f
        }

        val spanned = text as? Spanned ?: return paint.measureText(text, start, end)
        val basePaint = TextPaint(paint)
        var measuredWidth = 0f
        var currentStart = start
        while (currentStart < end) {
            val currentEnd = spanned.nextSpanTransition(currentStart, end, MetricAffectingSpan::class.java)
            val runPaint = TextPaint(basePaint)
            spanned.getSpans(currentStart, currentEnd, MetricAffectingSpan::class.java)
                .forEach { span -> span.updateMeasureState(runPaint) }
            measuredWidth += runPaint.measureText(text, currentStart, currentEnd)
            currentStart = currentEnd
        }
        return measuredWidth
    }

    private fun resolveDensity(paint: Paint): Float {
        val textPaintDensity = (paint as? TextPaint)?.density ?: 0f
        return textPaintDensity.takeIf { it > 0f } ?: Resources.getSystem().displayMetrics.density
    }
}
