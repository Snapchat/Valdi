package com.snap.valdi.attributes.impl.richtext

import android.graphics.Color
import android.graphics.Paint
import android.text.TextPaint
import android.text.style.CharacterStyle
import android.text.style.ReplacementSpan
import android.text.style.UpdateAppearance

class InvisibleReplacementSpan : ReplacementSpan() {
    override fun getSize(
        paint: Paint,
        text: CharSequence?,
        start: Int,
        end: Int,
        fm: Paint.FontMetricsInt?
    ): Int {
        if (fm != null) {
            paint.getFontMetricsInt(fm)
        }

        return if (text != null) {
            kotlin.math.ceil(paint.measureText(text, start, end).toDouble()).toInt()
        } else {
            0
        }
    }

    override fun draw(
        canvas: android.graphics.Canvas,
        text: CharSequence?,
        start: Int,
        end: Int,
        x: Float,
        top: Int,
        y: Int,
        bottom: Int,
        paint: Paint,
    ) {
    }
}

class InvisibleForegroundColorSpan : CharacterStyle(), UpdateAppearance {
    override fun updateDrawState(tp: TextPaint) {
        tp.color = Color.TRANSPARENT
        tp.bgColor = Color.TRANSPARENT
        tp.clearShadowLayer()
    }
}
