package com.snap.valdi.attributes.impl.richtext

import android.graphics.DashPathEffect
import android.graphics.PathEffect

class DashedUnderlineSpan : PatternUnderlineSpan() {
    override fun createPathEffect(strokeWidth: Float, density: Float): PathEffect {
        return DashPathEffect(floatArrayOf(strokeWidth * 3f, strokeWidth * 2f), 0f)
    }
}
