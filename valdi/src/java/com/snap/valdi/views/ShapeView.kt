package com.snap.valdi.views

import androidx.annotation.Keep
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.drawable.GradientDrawable
import android.view.View
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.GeometricPath
import com.snap.valdi.utils.PathInterpolator
import kotlin.math.min

private val DEFAULT_STROKE_WIDTH = 1.0f
private val DEFAULT_COLOR = Color.TRANSPARENT

@Keep
class ShapeView(context: Context) : View(context), ValdiRecyclableView {

    companion object {
        private const val TAG = "ShapeView"
    }

    var strokeStart: Float = 0.0f
        set(value) {
            if (field != value) {
                field = value
                invalidate()
            }
        }

    var strokeEnd: Float = 1.0f
        set(value) {
            if (field != value) {
                field = value
                invalidate()
            }
        }

    private val geometricPath = GeometricPath()
    private val strokePaint = Paint()
    private val fillPaint = Paint()
    private val fillGradientBounds = RectF()
    private val coordinateResolver = CoordinateResolver(context)
    private var pathInterpolator: PathInterpolator? = null
    private var fillColor = DEFAULT_COLOR
    private var fillGradient: ValdiGradient? = null
    private var fillGradientDirty = false
    private var fillGradientWidth = -1
    private var fillGradientHeight = -1

    init {
        strokePaint.strokeJoin = Paint.Join.MITER
        strokePaint.strokeCap = Paint.Cap.BUTT
        strokePaint.style = Paint.Style.STROKE

        fillPaint.style = Paint.Style.FILL

        strokePaint.isAntiAlias = true
        fillPaint.isAntiAlias = true

        resetStrokeColor()
        resetFillColor()
        resetStrokeWidth()
        resetStrokeCap()
        resetStrokeJoin()
    }

    fun setPathData(pathData: ByteArray?) {
        geometricPath.setPathData(pathData)
        pathInterpolator?.reset()
        invalidate()
    }

    fun setStrokeWidth(strokeWidth: Float) {
        strokePaint.strokeWidth = coordinateResolver.toPixelF(strokeWidth)
        invalidate()
    }

    fun resetStrokeWidth() {
        setStrokeWidth(DEFAULT_STROKE_WIDTH)
    }

    fun setStrokeColor(strokeColor: Int) {
        strokePaint.color = strokeColor
        invalidate()
    }

    fun resetStrokeColor() {
        setStrokeColor(DEFAULT_COLOR)
    }

    fun setFillColor(fillColor: Int) {
        this.fillColor = fillColor
        if (fillGradient == null) {
            fillPaint.color = fillColor
        }
        invalidate()
    }

    fun resetFillColor() {
        setFillColor(DEFAULT_COLOR)
    }

    fun setFillGradient(fillGradient: ValdiGradient) {
        this.fillGradient = fillGradient
        fillGradientDirty = true
        invalidate()
    }

    fun resetFillGradient() {
        fillGradient = null
        fillGradientDirty = false
        fillGradientWidth = -1
        fillGradientHeight = -1
        fillPaint.color = fillColor
        fillPaint.shader = null
        invalidate()
    }

    fun setStrokeJoin(strokeJoin: Paint.Join) {
        strokePaint.strokeJoin = strokeJoin
        invalidate()
    }

    fun resetStrokeJoin() {
        setStrokeJoin(Paint.Join.MITER)
    }

    fun setStrokeCap(strokeCap: Paint.Cap) {
        strokePaint.strokeCap = strokeCap
        invalidate()
    }

    fun resetStrokeCap() {
        setStrokeCap(Paint.Cap.BUTT)
    }

    override fun onDraw(canvas: Canvas) {
        ViewUtils.onDraw(this, canvas) {
            super.onDraw(it)

            if (geometricPath.isEmpty) {
                return
            }

            val activePath = this.getActivePath()

            if (fillGradient != null) {
                fillPaint.color = Color.BLACK
                updateFillGradientShader()
            } else {
                fillPaint.color = fillColor
                fillPaint.shader = null
            }

            it.drawPath(activePath, fillPaint)
            it.drawPath(activePath, strokePaint)
        }
    }

    private fun updateFillGradientShader() {
        val gradient = fillGradient ?: return
        if (!fillGradientDirty && fillGradientWidth == width && fillGradientHeight == height) {
            return
        }

        fillGradientDirty = false
        fillGradientWidth = width
        fillGradientHeight = height
        fillGradientBounds.set(0.0f, 0.0f, width.toFloat(), height.toFloat())

        when (gradient.getDrawableGradientType()) {
            GradientDrawable.LINEAR_GRADIENT -> {
                val x0: Float
                val x1: Float
                val y0: Float
                val y1: Float

                when (gradient.getDrawableOrientation()) {
                    GradientDrawable.Orientation.TOP_BOTTOM -> {
                        x0 = fillGradientBounds.left
                        y0 = fillGradientBounds.top
                        x1 = x0
                        y1 = fillGradientBounds.bottom
                    }
                    GradientDrawable.Orientation.TR_BL -> {
                        x0 = fillGradientBounds.right
                        y0 = fillGradientBounds.top
                        x1 = fillGradientBounds.left
                        y1 = fillGradientBounds.bottom
                    }
                    GradientDrawable.Orientation.RIGHT_LEFT -> {
                        x0 = fillGradientBounds.right
                        y0 = fillGradientBounds.top
                        x1 = fillGradientBounds.left
                        y1 = y0
                    }
                    GradientDrawable.Orientation.BR_TL -> {
                        x0 = fillGradientBounds.right
                        y0 = fillGradientBounds.bottom
                        x1 = fillGradientBounds.left
                        y1 = fillGradientBounds.top
                    }
                    GradientDrawable.Orientation.BOTTOM_TOP -> {
                        x0 = fillGradientBounds.left
                        y0 = fillGradientBounds.bottom
                        x1 = x0
                        y1 = fillGradientBounds.top
                    }
                    GradientDrawable.Orientation.BL_TR -> {
                        x0 = fillGradientBounds.left
                        y0 = fillGradientBounds.bottom
                        x1 = fillGradientBounds.right
                        y1 = fillGradientBounds.top
                    }
                    GradientDrawable.Orientation.LEFT_RIGHT -> {
                        x0 = fillGradientBounds.left
                        y0 = fillGradientBounds.top
                        x1 = fillGradientBounds.right
                        y1 = y0
                    }
                    GradientDrawable.Orientation.TL_BR -> {
                        x0 = fillGradientBounds.left
                        y0 = fillGradientBounds.top
                        x1 = fillGradientBounds.right
                        y1 = fillGradientBounds.bottom
                    }
                }

                fillPaint.shader = LinearGradient(
                    x0,
                    y0,
                    x1,
                    y1,
                    gradient.colors,
                    gradient.locations,
                    Shader.TileMode.CLAMP,
                )
            }
            GradientDrawable.RADIAL_GRADIENT -> {
                fillPaint.shader = RadialGradient(
                    fillGradientBounds.centerX(),
                    fillGradientBounds.centerY(),
                    min(fillGradientBounds.width(), fillGradientBounds.height()) / 2.0f,
                    gradient.colors,
                    gradient.locations,
                    Shader.TileMode.CLAMP,
                )
            }
            else -> {
                fillPaint.shader = null
            }
        }
    }

    private fun getActivePath(): Path {
        geometricPath.width = width
        geometricPath.height = height
        val path = geometricPath.path
        if (strokeStart == 0.0f && strokeEnd == 1.0f) {
            return path
        }

        var pathInterpolator = this.pathInterpolator
        if (pathInterpolator == null) {
            pathInterpolator = PathInterpolator()
            this.pathInterpolator = pathInterpolator
        }

        if (pathInterpolator.empty) {
            pathInterpolator.setPath(path)
        }

        return pathInterpolator.interpolate(strokeStart, strokeEnd)
    }

    override fun hasOverlappingRendering(): Boolean {
        return ViewUtils.hasOverlappingRendering(this)
    }

}
