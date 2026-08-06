package com.snap.valdi.attributes.impl

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.widget.TextView
import com.snap.valdi.attributes.impl.richtext.TextAlignment
import com.snap.valdi.views.ValdiTextHolder

class ValdiTextViewBackgroundEffectsLayoutManager(
    private val textView: TextView,
    private val textHolder: ValdiTextHolder
) {
    private val backgroundPaint by lazy {
        Paint().apply {
            color = Color.TRANSPARENT
            style = Paint.Style.FILL
            isAntiAlias = true
        }
    }
    private val backgroundPath by lazy { Path() }
    private val lineBounds by lazy { Rect() }

    fun drawBackgroundEffects(
        canvas: Canvas,
        backgroundEffects: ValdiTextViewBackgroundEffects,
    ) {
        val lineCount = textView.lineCount
        if (textView.lineCount == 0 || backgroundEffects.color == Color.TRANSPARENT) {
            return
        }

        backgroundPaint.color = backgroundEffects.color
        val lineBounds = mutableListOf<RectF>()
        val padding = backgroundEffects.padding.toFloat()
        for (lineIndex in 0 until lineCount) {
            val bounds = RectF()
            calculateLineBounds(lineIndex, padding, bounds)
            lineBounds.add(RectF(bounds))
        }

        // Adjust line widths so that they are the same if they are close; ensure there's enough
        // room to draw the rounded corner arcs between rows
        val cornerRadius = backgroundEffects.borderRadius
        val minDelta = cornerRadius * if (isLeftAligned() || isRightAligned()) {
            CORNER_COUNT_LEFT_RIGHT
        } else {
            CORNER_COUNT_CENTER
        }
        for (lineIndex in 1 until lineCount) {
            val currentLine = lineBounds[lineIndex]
            val lineAbove = lineBounds[lineIndex - 1]
            val lineDiff = lineAbove.width() - currentLine.width()
            if (lineDiff > 0 && lineDiff < minDelta) {
                currentLine.left = lineAbove.left
                currentLine.right = lineAbove.right
            } else if (lineDiff < 0 && -lineDiff < minDelta) {
                if (isLeftAligned() || !isRightAligned()) {
                    currentLine.right = lineAbove.right + cornerRadius * 2
                }
                if (isRightAligned() || !isLeftAligned()) {
                    currentLine.left = lineAbove.left - cornerRadius * 2
                }
            }
        }

        // Create a clockwise path around the caption by iterating from top to bottom on the right
        // side and then from bottom to top on the left side.
        val path = backgroundPath
        var yPos = lineBounds[0].top
        val firstLine = lineBounds[0]
        path.reset()
        path.moveTo(firstLine.left + cornerRadius, yPos)
        path.lineTo(firstLine.right - cornerRadius, yPos)
        path.arcTo(
            firstLine.right - cornerRadius, yPos + cornerRadius, cornerRadius,
            CornerType.TOP_RIGHT_OUTSIDE
        )

        // Down the right side
        for (lineIndex in 0 until lineCount) {
            val currentLine = lineBounds[lineIndex]
            if (lineIndex < lineCount - 1) {
                val lineBelow = lineBounds[lineIndex + 1]
                if (lineBelow.right == currentLine.right) {
                    yPos = currentLine.bottom
                    path.lineTo(currentLine.right, yPos)
                } else if (lineBelow.right > currentLine.right) {
                    yPos = lineBelow.top
                    path.lineTo(currentLine.right, yPos - cornerRadius)
                    path.arcTo(
                        currentLine.right + cornerRadius, yPos - cornerRadius, cornerRadius,
                        CornerType.BOTTOM_LEFT_INSIDE
                    )
                    path.lineTo(lineBelow.right - cornerRadius, yPos)
                    path.arcTo(
                        lineBelow.right - cornerRadius, yPos + cornerRadius, cornerRadius,
                        CornerType.TOP_RIGHT_OUTSIDE
                    )
                } else {
                    yPos = currentLine.bottom
                    path.lineTo(currentLine.right, yPos - cornerRadius)
                    path.arcTo(
                        currentLine.right - cornerRadius, yPos - cornerRadius, cornerRadius,
                        CornerType.BOTTOM_RIGHT_OUTSIDE
                    )
                    path.lineTo(lineBelow.right + cornerRadius, yPos)
                    path.arcTo(
                        lineBelow.right + cornerRadius, yPos + cornerRadius, cornerRadius,
                        CornerType.TOP_LEFT_INSIDE
                    )
                }
            } else {
                yPos = currentLine.bottom
                path.lineTo(currentLine.right, yPos - cornerRadius)
                path.arcTo(
                    currentLine.right - cornerRadius, yPos - cornerRadius, cornerRadius,
                    CornerType.BOTTOM_RIGHT_OUTSIDE
                )
            }
        }

        // Across the bottom
        val bottomLine = lineBounds[lineCount - 1]
        path.lineTo(bottomLine.left + cornerRadius, yPos)
        path.arcTo(
            bottomLine.left + cornerRadius, yPos - cornerRadius, cornerRadius,
            CornerType.BOTTOM_LEFT_OUTSIDE
        )

        // Up the left side
        for (lineIndex in lineCount - 1 downTo 0) {
            val currentLine = lineBounds[lineIndex]
            if (lineIndex > 0) {
                val lineAbove = lineBounds[lineIndex - 1]
                if (lineAbove.left == currentLine.left) {
                    yPos = currentLine.top
                    path.lineTo(currentLine.left, yPos)
                } else if (lineAbove.left < currentLine.left) {
                    yPos = lineAbove.bottom
                    path.lineTo(currentLine.left, yPos + cornerRadius)
                    path.arcTo(
                        currentLine.left - cornerRadius, yPos + cornerRadius, cornerRadius,
                        CornerType.TOP_RIGHT_INSIDE
                    )
                    path.lineTo(lineAbove.left + cornerRadius, yPos)
                    path.arcTo(
                        lineAbove.left + cornerRadius, yPos - cornerRadius, cornerRadius,
                        CornerType.BOTTOM_LEFT_OUTSIDE
                    )
                } else {
                    yPos = currentLine.top
                    path.lineTo(currentLine.left, yPos + cornerRadius)
                    path.arcTo(
                        currentLine.left + cornerRadius, yPos + cornerRadius, cornerRadius,
                        CornerType.TOP_LEFT_OUTSIDE
                    )
                    path.lineTo(lineAbove.left - cornerRadius, yPos)
                    path.arcTo(
                        lineAbove.left - cornerRadius, yPos - cornerRadius, cornerRadius,
                        CornerType.BOTTOM_RIGHT_INSIDE
                    )
                }
            } else {
                yPos = currentLine.top
                path.lineTo(currentLine.left, yPos + cornerRadius)
                path.arcTo(
                    currentLine.left + cornerRadius, yPos + cornerRadius, cornerRadius,
                    CornerType.TOP_LEFT_OUTSIDE
                )
            }
        }
        canvas.drawPath(path, backgroundPaint)
    }

    private enum class CornerType(val startAngle: Float, val sweepAngle: Float) {
        TOP_RIGHT_OUTSIDE(270f, 90f),
        TOP_RIGHT_INSIDE(0f, -90f),
        TOP_LEFT_OUTSIDE(180f, 90f),
        TOP_LEFT_INSIDE(270f, -90f),
        BOTTOM_RIGHT_OUTSIDE(0f, 90f),
        BOTTOM_RIGHT_INSIDE(90f, -90f),
        BOTTOM_LEFT_OUTSIDE(90f, 90f),
        BOTTOM_LEFT_INSIDE(180f, -90f)
    }

    private fun Path.arcTo(
        centerX: Float,
        centerY: Float,
        cornerRadius: Float,
        cornerType: CornerType
    ) {
        arcTo(
            centerX - cornerRadius,
            centerY - cornerRadius,
            centerX + cornerRadius,
            centerY + cornerRadius,
            cornerType.startAngle,
            cornerType.sweepAngle,
            false
        )
    }

    fun isRightAligned(): Boolean {
        return textHolder.textViewHelper?.fontAttributes?.alignment == TextAlignment.RIGHT
    }

    fun isLeftAligned(): Boolean {
        return (textHolder.textViewHelper?.fontAttributes?.alignment
            ?: TextAlignment.LEFT) == TextAlignment.LEFT
    }

    private fun calculateLineBounds(
        lineIndex: Int,
        padding: Float,
        bounds: RectF,
    ) {
        val layout = textView.layout ?: return
        textView.getLineBounds(lineIndex, lineBounds)
        var top = lineBounds.top
        var bottom = lineBounds.bottom

        val lineWidth = layout.getLineMax(lineIndex)
        val left = getLeft(lineWidth, lineBounds)
        val right = left + lineWidth

        bounds.left = left - padding
        bounds.top = top - padding
        bounds.right = right + padding
        bounds.bottom = bottom + padding
    }

    private fun getLeft(lineWidth: Float, bounds: Rect): Float {
        return when {
            isRightAligned() -> bounds.right - lineWidth
            isLeftAligned() -> bounds.left.toFloat()
            else -> (bounds.left + bounds.right - lineWidth) * 0.5f
        }
    }

    companion object {
        private const val CORNER_COUNT_LEFT_RIGHT = 2
        private const val CORNER_COUNT_CENTER = 4
    }
}

class ValdiTextViewBackgroundEffects() {
    var color: Int = Color.TRANSPARENT
    var borderRadius: Float = 0f
    var padding: Double = 0.0
}
