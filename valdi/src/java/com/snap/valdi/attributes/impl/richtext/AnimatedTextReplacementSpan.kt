package com.snap.valdi.attributes.impl.richtext

import android.graphics.Canvas
import android.graphics.Paint
import android.text.style.ReplacementSpan
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import kotlin.math.roundToInt

class AnimatedTextReplacementSpan(
    private val animation: AttributedTextAnimation,
    private val attributes: FontAttributes,
    private val fontManager: FontManager,
    private val missingFontsTracker: MissingFontsTracker,
    private val density: Float
) : ReplacementSpan() {
    override fun getSize(
        paint: Paint,
        text: CharSequence?,
        start: Int,
        end: Int,
        fm: Paint.FontMetricsInt?
    ): Int {
        if (fm != null) {
            val metrics = paint.fontMetricsInt
            fm.ascent = metrics.ascent
            fm.descent = metrics.descent
            fm.top = metrics.top
            fm.bottom = metrics.bottom
        }

        return paint.measureText(text, start, end).roundToInt()
    }

    override fun draw(
        canvas: Canvas,
        text: CharSequence?,
        start: Int,
        end: Int,
        x: Float,
        top: Int,
        y: Int,
        bottom: Int,
        paint: Paint
    ) {
        if (text == null || start >= end) {
            return
        }

        val alpha = (animation.opacity * paint.alpha).roundToInt().coerceIn(0, 255)
        if (alpha <= 0) {
            return
        }

        val width = paint.measureText(text, start, end)
        val centerX = x + width / 2f
        val centerY = (top + bottom) / 2f

        canvas.save()
        canvas.translate(centerX, centerY + animation.translationY * density)
        canvas.scale(animation.scale, animation.scale)
        canvas.translate(-centerX, -centerY)

        if (attributes.outlineColor != null && attributes.outlineWidth > 0f) {
            val outlinePaint = Paint(attributes.toPaint(fontManager, missingFontsTracker))
            outlinePaint.alpha = alpha
            canvas.drawText(text, start, end, x, y.toFloat(), outlinePaint)
        }

        val fillPaint = Paint(attributes.toFillPaint(fontManager, missingFontsTracker))
        fillPaint.alpha = alpha
        canvas.drawText(text, start, end, x, y.toFloat(), fillPaint)
        canvas.restore()
    }
}
