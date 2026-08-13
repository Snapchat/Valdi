package com.snap.valdi.views

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.RadialGradient
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.ShapeViewAttributesBinder
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ShapeViewTest {

    @Test
    fun singleColorFillGradientPreservesAndRestoresBoundFillColor() {
        val view = ShapeView(getApplicationContext())
        view.layout(0, 0, 100, 100)
        view.setPathData(rectangularPathData())
        view.setFillColor(Color.GREEN)

        applyFillGradient(view, arrayOf<Any>(0xFF0000FFL))
        draw(view)
        assertEquals(Color.RED, fillPaint(view).color)

        view.resetFillGradient()
        draw(view)
        assertEquals(Color.GREEN, fillPaint(view).color)

        applyFillGradient(view, arrayOf<Any>(0xFF0000FFL))
        view.setFillColor(Color.BLUE)
        draw(view)
        assertEquals(Color.RED, fillPaint(view).color)

        view.resetFillGradient()
        draw(view)
        assertEquals(Color.BLUE, fillPaint(view).color)
    }

    @Test
    fun radialFillGradientWaitsForNonzeroBounds() {
        for ((width, height) in listOf(0 to 100, 100 to 0)) {
            val view = ShapeView(getApplicationContext())
            view.layout(0, 0, width, height)
            view.setFillGradient(ValdiGradient(intArrayOf(Color.RED, Color.BLUE), null, true))

            updateFillGradientShader(view)
            assertNull(fillPaint(view).shader)

            view.layout(0, 0, 100, 100)
            updateFillGradientShader(view)
            assertTrue(fillPaint(view).shader is RadialGradient)

            view.layout(0, 0, width, height)
            updateFillGradientShader(view)
            assertNull(fillPaint(view).shader)

            view.layout(0, 0, 100, 100)
            updateFillGradientShader(view)
            assertTrue(fillPaint(view).shader is RadialGradient)
        }
    }

    @Test
    fun radialFillGradientScalesToRectangularBounds() {
        assertRadialGradientTransform(200, 100, floatArrayOf(100.0f, 50.0f, 200.0f, 50.0f, 100.0f, 100.0f))
        assertRadialGradientTransform(100, 200, floatArrayOf(50.0f, 100.0f, 100.0f, 100.0f, 50.0f, 200.0f))
    }

    private fun assertRadialGradientTransform(width: Int, height: Int, expectedPoints: FloatArray) {
        val view = ShapeView(getApplicationContext())
        view.layout(0, 0, width, height)
        view.setFillGradient(ValdiGradient(intArrayOf(Color.RED, Color.BLUE), null, true))
        updateFillGradientShader(view)

        val gradient = fillPaint(view).shader as RadialGradient
        val matrix = Matrix()
        assertTrue(gradient.getLocalMatrix(matrix))

        val points = floatArrayOf(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f)
        matrix.mapPoints(points)
        assertArrayEquals(expectedPoints, points, 0.001f)
    }

    private fun applyFillGradient(view: ShapeView, colors: Array<Any>) {
        val gradientData = arrayOf<Any>(colors, arrayOf<Any>(), 0, false)
        ShapeViewAttributesBinder::class.java.getDeclaredMethod(
            "applyFillGradient",
            ShapeView::class.java,
            Array<Any>::class.java,
            ValdiAnimator::class.java,
        ).apply {
            isAccessible = true
        }.invoke(ShapeViewAttributesBinder(), view, gradientData, null)
    }

    private fun updateFillGradientShader(view: ShapeView) {
        ShapeView::class.java.getDeclaredMethod("updateFillGradientShader").apply {
            isAccessible = true
        }.invoke(view)
    }

    private fun fillPaint(view: ShapeView): Paint {
        return ShapeView::class.java.getDeclaredField("fillPaint").apply {
            isAccessible = true
        }.get(view) as Paint
    }

    private fun draw(view: ShapeView) {
        val bitmap = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
        view.draw(Canvas(bitmap))
    }

    private fun rectangularPathData(): ByteArray {
        val values = doubleArrayOf(
            100.0, 100.0, 1.0,
            1.0, 0.0, 0.0,
            2.0, 100.0, 0.0,
            2.0, 100.0, 100.0,
            2.0, 0.0, 100.0,
            7.0,
        )
        val buffer = ByteBuffer.allocate(values.size * Double.SIZE_BYTES).order(ByteOrder.LITTLE_ENDIAN)
        for (value in values) {
            buffer.putDouble(value)
        }
        return buffer.array()
    }
}
