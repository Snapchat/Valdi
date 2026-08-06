package com.snap.valdi.attributes.impl.richtext

import android.graphics.Canvas
import android.graphics.Color
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

        // Opacity is a multiplier, not an absolute alpha. The previous form collapsed
        // animation.opacity and the layout paint's alpha into one 0-255 value and then assigned it
        // straight onto both paints, which discarded whatever transparency attributes.color and
        // attributes.outlineColor carried — a 50%-opaque outline animating at opacity 1.0 came out
        // fully opaque. Each paint now scales its own colour's alpha by this factor instead.
        val opacity = animation.opacity * (paint.alpha / 255f)
        if (opacity <= 0f) {
            return
        }

        val width = paint.measureText(text, start, end)
        val centerX = x + width / 2f
        val centerY = (top + bottom) / 2f

        val outlinePaint = outlinePaintForOpacity(opacity)
        val fillPaint = fillPaintForOpacity(opacity)

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

    /**
     * Scales [baseColor]'s own alpha by [opacity]. Derived from the attribute colour rather than the
     * paint, because the paints are cached across draws — reading back a previously-scaled
     * paint.alpha would compound the scaling frame after frame.
     */
    private fun scaledAlpha(baseColor: Int, opacity: Float): Int =
        (Color.alpha(baseColor) * opacity).roundToInt().coerceIn(0, 255)

    private fun outlinePaintForOpacity(opacity: Float): Paint? {
        val outlineColor = attributes.outlineColor
        if (outlineColor == null || attributes.outlineWidth <= 0f) {
            return null
        }

        val paint = outlinePaint ?: attributes.toPaint(fontManager, missingFontsTracker).also {
            outlinePaint = it
        }
        paint.alpha = scaledAlpha(outlineColor, opacity)
        return paint
    }

    private fun fillPaintForOpacity(opacity: Float): Paint {
        val paint = fillPaint ?: attributes.toFillPaint(fontManager, missingFontsTracker).also {
            fillPaint = it
        }
        paint.alpha = scaledAlpha(attributes.color, opacity)
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
