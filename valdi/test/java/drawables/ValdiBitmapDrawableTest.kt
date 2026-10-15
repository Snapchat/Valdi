package com.snap.valdi.drawables

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import com.snap.valdi.utils.CanvasClipper
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],manifest=Config.NONE)
internal class ValdiBitmapDrawableTest {

    private class RecordingCanvas: Canvas() {
        var drawRectCount = 0

        override fun drawRect(r: Rect, paint: Paint) {
            drawRectCount++
        }
    }

    private fun makeDrawable(bitmap: Bitmap): ValdiBitmapDrawable {
        return ValdiBitmapDrawable(CanvasClipper()).also {
            it.bitmap = bitmap
            it.setBounds(0, 0, 100, 100)
        }
    }

    @Test
    fun drawsLiveBitmap() {
        val drawable = makeDrawable(Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888))
        val canvas = RecordingCanvas()

        drawable.draw(canvas)

        assertEquals(1, canvas.drawRectCount)
    }

    // COMPOSER-6344: drawing a bitmap recycled by its owning ValdiImage must not reach the shader.
    @Test
    fun skipsRecycledBitmap() {
        val bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888)
        val drawable = makeDrawable(bitmap)
        val canvas = RecordingCanvas()

        bitmap.recycle()
        drawable.draw(canvas)

        assertEquals(0, canvas.drawRectCount)
    }
}
