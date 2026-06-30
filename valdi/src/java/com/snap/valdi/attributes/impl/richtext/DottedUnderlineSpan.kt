package com.snap.valdi.attributes.impl.richtext

import android.graphics.Path
import android.graphics.PathDashPathEffect
import android.graphics.PathEffect
import android.graphics.Paint

class DottedUnderlineSpan(animation: AttributedTextAnimation?) : PatternUnderlineSpan(animation) {
    companion object {
        private const val STROKE_WIDTH_DP = 2f
        private const val GAP_DP = 2f
        private const val OFFSET_DP = 0.5f
    }

    override fun resolveStrokeWidth(paint: android.graphics.Paint, density: Float): Float {
        return STROKE_WIDTH_DP * density
    }

    override fun resolveUnderlineY(
        paint: android.graphics.Paint,
        baseline: Int,
        strokeWidth: Float,
        density: Float
    ): Float {
        return baseline + strokeWidth + OFFSET_DP * density
    }

    override fun resolvePaintStyle(): Paint.Style {
        return Paint.Style.FILL
    }

    override fun createPathEffect(strokeWidth: Float, density: Float): PathEffect {
        val gap = (GAP_DP * density).coerceAtLeast(1f - strokeWidth)
        val radius = strokeWidth / 2f
        val dot = Path().apply {
            addCircle(0f, 0f, radius, Path.Direction.CW)
        }
        return PathDashPathEffect(dot, strokeWidth + gap, 0f, PathDashPathEffect.Style.TRANSLATE)
    }
}
