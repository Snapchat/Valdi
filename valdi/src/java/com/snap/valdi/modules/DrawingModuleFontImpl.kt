package com.snap.valdi.modules

import android.graphics.Paint
import android.graphics.Typeface
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import com.snap.valdi.modules.drawing.Font
import com.snap.valdi.modules.drawing.Size
import com.snap.valdi.utils.CoordinateResolver
import kotlin.math.max
import kotlin.math.min

class DrawingModuleFontImpl(typeface: Typeface,
                            val fontSizePixels: Float,
                            lineHeight: Double?,
                            private val coordinateResolver: CoordinateResolver): Font {

    private val paint = TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        this.typeface = typeface
        this.textSize = fontSizePixels
    }

    private val lineSpacing = lineHeight?.toFloat() ?: 1f

    override fun measureText(text: String, maxWidth: Double?, maxHeight: Double?, maxLines: Double?): Size {
        // TODO(simon): Measuring does not exactly match iOS, we have additional padding added on
        //  the sides for multiline text.

        val width = maxWidth?.let { coordinateResolver.toPixel(it) } ?: Int.MAX_VALUE

        val layout = StaticLayout(text, paint, width, Layout.Alignment.ALIGN_NORMAL, lineSpacing, 0f, false)

        val lineCount = maxLines?.let { min(layout.lineCount, it.toInt()) } ?: layout.lineCount
        var textWidth = 0f
        for (i in 0 until lineCount) {
            val lineWidth = layout.getLineWidth(i)
            textWidth = max(textWidth, lineWidth)
        }

        // https://developer.android.com/reference/android/text/StaticLayout#getLineTop(int)
        // If the specified line is equal to the line count, returns the bottom of the last line.
        val textHeight = layout.getLineTop(lineCount)

        val layoutWidth = coordinateResolver.fromPixel(textWidth)
        val layoutHeight = coordinateResolver.fromPixel(textHeight)

        return Size(layoutWidth, layoutHeight)
    }

}
