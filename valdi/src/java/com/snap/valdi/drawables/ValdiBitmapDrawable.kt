package com.snap.valdi.drawables

import android.graphics.Bitmap
import android.graphics.BitmapShader
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.ColorFilter
import android.graphics.ColorMatrixColorFilter
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.drawable.Drawable
import android.widget.ImageView
import com.snap.valdi.utils.CanvasClipper

/**
 * A bitmap drawable implementation that can be re-used efficiently.
 */
class ValdiBitmapDrawable(val clipper: CanvasClipper): Drawable() {

    var clipsToBound = true
        set(value) {
            if (value != field) {
                field = value
                invalidateSelf()
                setMatrixDirty()
            }
        }

    var scaleType = ImageView.ScaleType.FIT_XY
        set(value) {
            if (field != value) {
                field = value
                invalidateSelf()
                setMatrixDirty()
            }
        }

    var contentScaleX = 1.0f
        set(value) {
            if (field != value) {
                field = value
                invalidateSelf()
                setMatrixDirty()
            }
        }

    var contentScaleY = 1.0f
        set(value) {
            if (field != value) {
                field = value
                invalidateSelf()
                setMatrixDirty()
            }
        }

    var contentRotation = 0.0f
        set(value) {
            if (field != value) {
                field = value
                invalidateSelf()
                setMatrixDirty()
            }
        }

    var flip = false
        set(value) {
            if (field != value) {
                field = value
                invalidateSelf()
                setMatrixDirty()
            }
        }

    var colorMatrix: FloatArray? = null
        set(value) {
            if (field !== value) {
                field = value

                if (value == null) {
                    this.paint.colorFilter = null
                } else {
                    this.paint.colorFilter = ColorMatrixColorFilter(value)
                }

                invalidateSelf()
            }
        }

    var bitmap: Bitmap?  = null
        set(value) {
            if (value != field) {
                field = value

                if (value != null) {
                    this.paint.shader = BitmapShader(value, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP)
                    if (value.width != bitmapWidth || value.height != bitmapHeight) {
                        bitmapWidth = value.width
                        bitmapHeight = value.height
                        setMatrixDirty()
                    } else if (!matrixDirty) {
                        this.paint.shader.setLocalMatrix(transformationMatrix)
                    }
                } else {
                    this.paint.shader = null
                    // We don't invalidate the matrix in case we will get a new bitmap with the same size as before
                }

                invalidateSelf()
            }
        }

    private val paint = Paint().also {
        it.isAntiAlias = true
        it.isFilterBitmap = true
    }

    private val transformationMatrix = Matrix()

    private val targetRectValue = Rect(0, 0, 0, 0)
    private var targetNeedClipping = false

    private var resolvedWidthScale = 1f
    private var resolvedHeightScale = 1f
    private var bitmapWidth = 0
    private var bitmapHeight = 0
    private var matrixDirty = true
    private val cachedTransformedSize = RectF()

    private var tintColor = Color.TRANSPARENT

    private fun setMatrixDirty() {
        matrixDirty = true
    }

    override fun setTint(tintColor: Int) {
        if (this.tintColor != tintColor) {
            this.tintColor = tintColor

            if (tintColor != Color.TRANSPARENT) {
                paint.colorFilter = PorterDuffColorFilter(tintColor, PorterDuff.Mode.SRC_IN)
            } else {
                paint.colorFilter = null
            }

            invalidateSelf()
        }
    }

    override fun onBoundsChange(bounds: Rect) {
        super.onBoundsChange(bounds)

        setMatrixDirty()
    }

    override fun draw(canvas: Canvas) {
        this.bitmap ?: return

        if (matrixDirty) {
            matrixDirty = false
            updateMatrixAndDrawRect(bitmapWidth, bitmapHeight)
        }

        if (clipper.hasNonNullCornerRadius) {
            if (targetNeedClipping) {
                canvas.clipRect(targetRectValue)
            }
            canvas.drawPath(clipper.getPath(bounds), paint)
        } else {
            canvas.drawRect(targetRectValue, paint)
        }
    }

    override fun setAlpha(alpha: Int) {
        // Unsupported
    }

    override fun getOpacity(): Int {
        return PixelFormat.TRANSPARENT
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        // Unsupported
    }

    override fun getIntrinsicWidth(): Int {
        return bitmapWidth
    }

    override fun getIntrinsicHeight(): Int {
        return bitmapHeight
    }

    private fun updateDrawRect(left: Int, top: Int, right: Int, bottom: Int, needsClipping: Boolean) {
        val offsetX = bounds.left
        val offsetY = bounds.top
        targetRectValue.set(offsetX + left, offsetY + top, offsetX + right, offsetY + bottom)
        targetNeedClipping = needsClipping
    }

    private fun updateMatrixWithContentTransform(imageWidthF: Float, imageHeightF: Float, outputSize: RectF) {
        val isTransformed = contentScaleX != 1f || contentScaleY != 1f || contentRotation != 0f

        if (!isTransformed) {
            outputSize.set(0f, 0f, imageWidthF, imageHeightF)
            transformationMatrix.mapRect(outputSize)
            return
        }

        val thisWidth = this.bounds.width()
        val thisHeight = this.bounds.height()

        val widthF = thisWidth.toFloat()
        val heightF = thisHeight.toFloat()

        val centerX = widthF / 2
        val centerY = heightF / 2

        // this takes degrees, valdi works in radians
        transformationMatrix.postScale(contentScaleX, contentScaleY, centerX, centerY)
        transformationMatrix.postRotate(Math.toDegrees(contentRotation.toDouble()).toFloat(), centerX, centerY)

        outputSize.set(0f, 0f, imageWidthF, imageHeightF)
        transformationMatrix.mapRect(outputSize)
    }

    private fun updateMatrixAndDrawRect(imageWidth: Int, imageHeight: Int) {
        val imageWidthF = imageWidth.toFloat()
        val imageHeightF = imageHeight.toFloat()

        val thisWidth = this.bounds.width()
        val thisHeight = this.bounds.height()

        if (scaleType != ImageView.ScaleType.CENTER_CROP &&
                scaleType != ImageView.ScaleType.CENTER &&
                scaleType != ImageView.ScaleType.FIT_XY &&
                scaleType != ImageView.ScaleType.CENTER_INSIDE) {
            transformationMatrix.reset()
            updateMatrixWithContentTransform(imageWidthF, imageHeightF, cachedTransformedSize)
            updateDrawRect(0, 0, cachedTransformedSize.width().toInt(), cachedTransformedSize.height().toInt(), false)

            if (flip) {
                transformationMatrix.preScale(-1f, 1f, cachedTransformedSize.width() / 2f, 0f)
            }
            if (paint.shader != null) {
                paint.shader.setLocalMatrix(transformationMatrix)
            }
            return
        }

        if (imageWidth == 0 || imageHeight == 0) {
            updateDrawRect(0, 0, 0, 0, false)
            return
        }

        val widthF = thisWidth.toFloat()
        val heightF = thisHeight.toFloat()

        resolveScale(widthF / imageWidthF, heightF / imageHeightF)

        transformationMatrix.reset()
        if (flip) {
            transformationMatrix.preScale(-1f, 1f, imageWidthF / 2f, 0f)
        }

        transformationMatrix.postTranslate(-imageWidthF / 2f, -imageHeightF / 2f)
        transformationMatrix.postScale(resolvedWidthScale, resolvedHeightScale)
        transformationMatrix.postTranslate(widthF / 2f, heightF / 2f)
        updateMatrixWithContentTransform(imageWidthF, imageHeightF, cachedTransformedSize)

        var widthOffset = thisWidth - cachedTransformedSize.width().toInt()
        var heightOffset = thisHeight - cachedTransformedSize.height().toInt()
        if (clipsToBound) {
            widthOffset = Math.max(widthOffset, 0)
            heightOffset = Math.max(heightOffset, 0)
        }

        updateDrawRect(
            widthOffset / 2,
            heightOffset / 2,
            thisWidth - widthOffset / 2,
            thisHeight - heightOffset / 2,
            widthOffset > 0 || heightOffset > 0
        )

        if (paint.shader != null) {
            paint.shader.setLocalMatrix(transformationMatrix)
        }
    }

    private fun resolveScale(widthScale: Float, heightScale: Float) {
        if (scaleType == ImageView.ScaleType.CENTER_CROP) {
            val scale = Math.max(widthScale, heightScale)
            resolvedWidthScale = scale
            resolvedHeightScale = scale
        } else if (scaleType == ImageView.ScaleType.CENTER) {
            resolvedWidthScale = 1f
            resolvedHeightScale = 1f
        } else if (scaleType == ImageView.ScaleType.FIT_XY) {
            resolvedWidthScale = widthScale
            resolvedHeightScale = heightScale
        } else if (scaleType == ImageView.ScaleType.CENTER_INSIDE) {
            val scale = Math.min(widthScale, heightScale)
            resolvedWidthScale = scale
            resolvedHeightScale = scale
        } else {
            resolvedWidthScale = 1f
            resolvedHeightScale = 1f
        }
    }

}
