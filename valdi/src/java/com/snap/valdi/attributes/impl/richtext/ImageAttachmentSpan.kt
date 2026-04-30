package com.snap.valdi.attributes.impl.richtext

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.text.style.ReplacementSpan
import androidx.annotation.Keep

@Keep
class ImageAttachmentInfo(
    val width: Float,
    val height: Float,
    val imageData: ByteArray?
)

@Keep
class ImageAttachmentSpan(
    private val imageInfo: ImageAttachmentInfo,
    private val density: Float
) : ReplacementSpan() {

    private var bitmap: Bitmap? = null
    private val layoutWidth: Int = (imageInfo.width * density).toInt()
    private val layoutHeight: Int = (imageInfo.height * density).toInt()

    init {
        imageInfo.imageData?.let { data ->
            bitmap = BitmapFactory.decodeByteArray(data, 0, data.size)
        }
    }

    override fun getSize(paint: Paint, text: CharSequence?, start: Int, end: Int, fm: Paint.FontMetricsInt?): Int {
        if (fm != null) {
            val fontMetrics = paint.fontMetrics
            val textCenter = (fontMetrics.descent + fontMetrics.ascent) / 2
            val imageTop = (textCenter - layoutHeight / 2).toInt()
            val imageBottom = imageTop + layoutHeight
            if (imageTop < fm.ascent) fm.ascent = imageTop
            if (imageTop < fm.top) fm.top = imageTop
            if (imageBottom > fm.descent) fm.descent = imageBottom
            if (imageBottom > fm.bottom) fm.bottom = imageBottom
        }
        return layoutWidth
    }

    override fun draw(canvas: Canvas, text: CharSequence?, start: Int, end: Int, x: Float, top: Int, y: Int, bottom: Int, paint: Paint) {
        val bmp = bitmap ?: return
        val fontMetrics = paint.fontMetrics
        val textCenter = (fontMetrics.descent + fontMetrics.ascent) / 2
        val drawY = y + textCenter - layoutHeight / 2
        canvas.drawBitmap(bmp, null, RectF(x, drawY, x + layoutWidth, drawY + layoutHeight), paint)
    }
}
