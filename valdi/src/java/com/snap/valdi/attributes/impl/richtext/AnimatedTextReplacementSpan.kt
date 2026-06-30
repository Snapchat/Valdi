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
    private var outlinePaint: Paint? = null
    private var fillPaint: Paint? = null
    private var patternUnderlineSpan: PatternUnderlineSpan? = null

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

        val outlinePaint = outlinePaintForAlpha(alpha)
        val fillPaint = fillPaintForAlpha(alpha)

        canvas.save()
        canvas.translate(centerX, centerY + animation.translationY * density)
        canvas.scale(animation.scale, animation.scale)
        canvas.translate(-centerX, -centerY)

        if (outlinePaint != null) {
            canvas.drawText(text, start, end, x, y.toFloat(), outlinePaint)
        }

        canvas.drawText(text, start, end, x, y.toFloat(), fillPaint)
        patternUnderlineSpan()?.drawUnderlineRange(canvas, fillPaint, x, x + width, top, y, bottom)
        canvas.restore()
    }

    private fun outlinePaintForAlpha(alpha: Int): Paint? {
        if (attributes.outlineColor == null || attributes.outlineWidth <= 0f) {
            return null
        }

        val paint = outlinePaint ?: attributes.toPaint(fontManager, missingFontsTracker).also {
            outlinePaint = it
        }
        paint.alpha = alpha
        return paint
    }

    private fun fillPaintForAlpha(alpha: Int): Paint {
        val paint = fillPaint ?: attributes.toFillPaint(fontManager, missingFontsTracker).also {
            fillPaint = it
        }
        paint.alpha = alpha
        return paint
    }

    private fun patternUnderlineSpan(): PatternUnderlineSpan? {
        if (!attributes.requiresDrawableUnderlineSpan()) {
            return null
        }

        return patternUnderlineSpan ?: attributes.createDrawableUnderlineSpan(null).also {
            patternUnderlineSpan = it
        }
    }
}
