package com.snap.valdi.attributes.impl.richtext

import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.PathEffect

class CustomUnderlineSpan(private val style: CustomUnderlineStyle) : PatternUnderlineSpan() {
    override fun resolveStrokeWidth(paint: Paint, density: Float): Float {
        return style.height * density
    }

    override fun resolveUnderlineY(
        paint: Paint,
        baseline: Int,
        strokeWidth: Float,
        density: Float
    ): Float {
        return super.resolveUnderlineY(paint, baseline, strokeWidth, density) + style.offset * density
    }

    override fun createPathEffect(strokeWidth: Float, density: Float): PathEffect? {
        if (!style.isPatterned) {
            return null
        }
        return DashPathEffect(floatArrayOf(style.onWidth * density, style.offWidth * density), 0f)
    }
}
