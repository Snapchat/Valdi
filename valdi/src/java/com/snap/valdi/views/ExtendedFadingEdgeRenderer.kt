package com.snap.valdi.views

import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.PorterDuff
import android.graphics.PorterDuffXfermode
import android.graphics.Shader

/**
 * Handles custom fading edge rendering for ValdiScrollView using Porter-Duff compositing.
 * This provides enhanced fading edges for larger fade lengths to match iOS visual appearance.
 */
internal class ExtendedFadingEdgeRenderer {
    private val fadingEdgePaint = Paint().apply {
        xfermode = PorterDuffXfermode(PorterDuff.Mode.DST_OUT)
    }
    
    private var startFadingEdgeShader: LinearGradient? = null
    private var endFadingEdgeShader: LinearGradient? = null
    private var lastFadingEdgeWidth: Int = 0
    private var lastFadingEdgeHeight: Int = 0

    fun drawVerticalFadingEdges(
        canvas: Canvas,
        width: Int,
        height: Int,
        fadeLength: Float,
        contentHeight: Int,
        contentOffsetY: Int,
        fadingEdgeStartEnabled: Boolean,
        fadingEdgeEndEnabled: Boolean
    ) {
        // Defensive checks: prevent divide-by-zero and invalid dimensions
        if (width <= 0 || height <= 0 || fadeLength <= 0 || fadeLength > height) return

        // Rebuild shaders if dimensions changed
        if (width != lastFadingEdgeWidth || height != lastFadingEdgeHeight) {
            invalidateShaders()
            lastFadingEdgeWidth = width
            lastFadingEdgeHeight = height
        }

        val maxScrollOffset = (contentHeight - height).coerceAtLeast(0)

        // Top fade: uses square root curve for faster initial appearance, smoother growth
        val startStrength = if (fadingEdgeStartEnabled && maxScrollOffset > 0 && contentOffsetY > 0) {
            val t = (contentOffsetY.toFloat() / fadeLength).coerceIn(0f, 1f)
            kotlin.math.sqrt(t)
        } else 0f

        // Bottom fade: visible at full strength as long as there's ANY content below
        val endStrength = if (fadingEdgeEndEnabled && maxScrollOffset > 0) {
            val remaining = maxScrollOffset - contentOffsetY
            if (remaining > 0) 1f else 0f
        } else 0f

        // Draw top fading edge
        if (startStrength > 0f) {
            if (startFadingEdgeShader == null) {
                startFadingEdgeShader = LinearGradient(
                    0f, 0f, 0f, fadeLength,
                    intArrayOf(0xFF000000.toInt(), 0x00000000),
                    floatArrayOf(0f, 1f),
                    Shader.TileMode.CLAMP
                )
            }
            fadingEdgePaint.shader = startFadingEdgeShader
            fadingEdgePaint.alpha = (255 * startStrength).toInt()
            canvas.drawRect(0f, 0f, width.toFloat(), fadeLength, fadingEdgePaint)
        }

        // Draw bottom fading edge
        if (endStrength > 0f) {
            if (endFadingEdgeShader == null) {
                endFadingEdgeShader = LinearGradient(
                    0f, height - fadeLength, 0f, height.toFloat(),
                    intArrayOf(0x00000000, 0xFF000000.toInt()),
                    floatArrayOf(0f, 1f),
                    Shader.TileMode.CLAMP
                )
            }
            fadingEdgePaint.shader = endFadingEdgeShader
            fadingEdgePaint.alpha = (255 * endStrength).toInt()
            canvas.drawRect(0f, height - fadeLength, width.toFloat(), height.toFloat(), fadingEdgePaint)
        }
    }

    fun drawHorizontalFadingEdges(
        canvas: Canvas,
        width: Int,
        height: Int,
        fadeLength: Float,
        contentWidth: Int,
        contentOffsetX: Int,
        fadingEdgeStartEnabled: Boolean,
        fadingEdgeEndEnabled: Boolean
    ) {
        // Defensive checks: prevent divide-by-zero and invalid dimensions
        if (width <= 0 || height <= 0 || fadeLength <= 0 || fadeLength > width) return

        // Rebuild shaders if dimensions changed
        if (width != lastFadingEdgeWidth || height != lastFadingEdgeHeight) {
            invalidateShaders()
            lastFadingEdgeWidth = width
            lastFadingEdgeHeight = height
        }

        val maxScrollOffset = (contentWidth - width).coerceAtLeast(0)

        // Left fade: uses square root curve for faster initial appearance
        val startStrength = if (fadingEdgeStartEnabled && maxScrollOffset > 0 && contentOffsetX > 0) {
            val t = (contentOffsetX.toFloat() / fadeLength).coerceIn(0f, 1f)
            kotlin.math.sqrt(t)
        } else 0f

        // Right fade: visible at full strength as long as there's ANY content to the right
        val endStrength = if (fadingEdgeEndEnabled && maxScrollOffset > 0) {
            val remaining = maxScrollOffset - contentOffsetX
            if (remaining > 0) 1f else 0f
        } else 0f

        // Draw left fading edge
        if (startStrength > 0f) {
            if (startFadingEdgeShader == null) {
                startFadingEdgeShader = LinearGradient(
                    0f, 0f, fadeLength, 0f,
                    intArrayOf(0xFF000000.toInt(), 0x00000000),
                    floatArrayOf(0f, 1f),
                    Shader.TileMode.CLAMP
                )
            }
            fadingEdgePaint.shader = startFadingEdgeShader
            fadingEdgePaint.alpha = (255 * startStrength).toInt()
            canvas.drawRect(0f, 0f, fadeLength, height.toFloat(), fadingEdgePaint)
        }

        // Draw right fading edge
        if (endStrength > 0f) {
            if (endFadingEdgeShader == null) {
                endFadingEdgeShader = LinearGradient(
                    width - fadeLength, 0f, width.toFloat(), 0f,
                    intArrayOf(0x00000000, 0xFF000000.toInt()),
                    floatArrayOf(0f, 1f),
                    Shader.TileMode.CLAMP
                )
            }
            fadingEdgePaint.shader = endFadingEdgeShader
            fadingEdgePaint.alpha = (255 * endStrength).toInt()
            canvas.drawRect(width - fadeLength, 0f, width.toFloat(), height.toFloat(), fadingEdgePaint)
        }
    }

    fun invalidateShaders() {
        startFadingEdgeShader = null
        endFadingEdgeShader = null
    }

    fun release() {
        invalidateShaders()
        fadingEdgePaint.shader = null
    }
}

