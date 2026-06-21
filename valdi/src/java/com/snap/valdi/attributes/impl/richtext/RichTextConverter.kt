package com.snap.valdi.attributes.impl.richtext

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.PointF
import android.graphics.RectF
import android.os.Build
import android.text.Layout
import android.text.Spannable
import android.text.SpannableStringBuilder
import android.text.StaticLayout
import android.text.TextDirectionHeuristic
import android.text.TextPaint
import android.text.TextUtils
import android.text.style.AlignmentSpan
import android.view.Gravity
import android.widget.TextView
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.gestures.TapContext
import com.snap.valdi.views.TextViewUtils

class RichTextConverter(val fontManager: FontManager) {
    class PartIndexSpan(val partIndex: Int)

    private val overlayBitmapPaint = TextPaint().apply {
        isFilterBitmap = true
    }
    private val layoutOrigin = PointF()

    data class ParsedAttributedText(
        val contentLengths: Array<Int>,
        val attributes: Array<FontAttributes>,
    )

    class OverlayLayoutCache(
        val overlaySpannable: Spannable,
        val width: Int,
        val alignment: Layout.Alignment,
        val lineSpacingExtra: Float,
        val lineSpacingMultiplier: Float,
        val includeFontPadding: Boolean,
        val breakStrategy: Int,
        val hyphenationFrequency: Int,
        val justificationMode: Int,
        val textDirection: Any?,
        val drawChunks: List<OverlayDrawChunk>,
        val drawChunksByPartIndex: Map<Int, List<OverlayDrawChunk>>,
        val lineCenterByIndex: Map<Int, Float>,
    ) {
        fun recycle() {
            drawChunks.forEach { chunk ->
                if (!chunk.bitmap.isRecycled) {
                    chunk.bitmap.recycle()
                }
            }
        }
    }

    data class OverlayDrawChunk(
        val partIndex: Int,
        val startOffset: Int,
        val chunkText: String,
        val x: Float,
        val baselineY: Float,
        val lineIndex: Int,
        val drawBounds: RectF,
        val bitmap: Bitmap,
        val bitmapLeft: Float,
        val bitmapTop: Float,
    )

    fun parseAttributedText(attributedText: AttributedText, startingAttributes: FontAttributes): ParsedAttributedText {
        val contentLengths = parseContentLengths(attributedText)
        val attributes = parseAttributes(attributedText, startingAttributes, contentLengths)
        return ParsedAttributedText(contentLengths, attributes)
    }

    private fun parseAttributes(attributedText: AttributedText, startingAttributes: FontAttributes, contentLengths: Array<Int>): Array<FontAttributes> {
        val partsSize = attributedText.getPartsSize()

        return Array(partsSize) {
            val attributes = startingAttributes.copy()
            val font = attributedText.getFontAtIndex(it)

            if (font != null) {
                attributes.applyFont(font)
            }

            val color = attributedText.getColorAtIndex(it)
            if (color != null) {
                attributes.color = color
            }

            val backgroundColor = attributedText.getBackgroundColorAtIndex(it)
            if (backgroundColor != null) {
                attributes.backgroundColor = backgroundColor
            }

            val outlineColor = attributedText.getOutlineColorAtIndex(it)
            if (outlineColor != null) {
                attributes.outlineColor = outlineColor
            }
            val outlineWidth = attributedText.getOutlineWidthAtIndex(it)
            if (outlineColor != null || outlineWidth > 0f) {
                attributes.outlineWidth = outlineWidth
            }
            attributes.animationTransform = attributedText.getAnimationTransformAtIndex(it)

            val textDecoration = attributedText.getTextDecorationAtIndex(it)
            if (textDecoration != null) {
                attributes.textDecoration = textDecoration
            }

            attributes
        }
    }

    private fun getIndexAtCharacter(character: Int, contentLengths: Array<Int>): Int {
        var total = 0
        for ((index, partLength) in contentLengths.withIndex()) {
            total += partLength
            if (total > character) {
                return index
            }
        }
        return contentLengths.size - 1
    }

    private fun parseContentLengths(attributedText: AttributedText): Array<Int> {
        val partsSize = attributedText.getPartsSize()
        return Array(partsSize) { attributedText.getContentAtIndex(it).length }
    }

    /**
     * Valdi a Valdi's AttributedText object into an Android SpannedString
     */
    fun convert(
        attributedText: AttributedText,
        startingAttributes: FontAttributes,
        missingFontsTracker: MissingFontsTracker,
        disableTextReplacement: Boolean = false,
        suppressAnimatedBase: Boolean = false,
        renderMode: FontAttributes.RenderMode = FontAttributes.RenderMode.BASE,
        density: Float = 1.0f
    ): Spannable {
        val contentLengths = parseContentLengths(attributedText)
        val attributes = parseAttributes(attributedText, startingAttributes, contentLengths)
        val spannable = SpannableStringBuilder()
        val hasAnimationTransform = attributedText.hasAnimationTransform()

        for ((index, attribute) in attributes.withIndex()) {
            val content = attributedText.getContentAtIndex(index)
            val start = spannable.length
            spannable.append(content)
            val end = spannable.length

            val onTap = attributedText.getOnTapAtIndex(index)
            val onLayout = attributedText.getOnLayoutAtIndex(index)
            val imageAttachment = attributedText.getImageAttachmentAtIndex(index)

            if ((hasAnimationTransform || renderMode == FontAttributes.RenderMode.OVERLAY) &&
                (renderMode != FontAttributes.RenderMode.OVERLAY || imageAttachment == null)
            ) {
                spannable.setSpan(
                    PartIndexSpan(index),
                    start,
                    end,
                    Spannable.SPAN_EXCLUSIVE_EXCLUSIVE,
                )
            }

            if (imageAttachment != null && !disableTextReplacement) {
                spannable.setSpan(ImageAttachmentSpan(imageAttachment, density),
                        start,
                        end,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                spannable.append("\u2009") // thin space for line break opportunity
            } else {
                attribute.enumerateSpans(
                    fontManager,
                    missingFontsTracker,
                    disableTextReplacement,
                    renderMode,
                    suppressAnimatedBase,
                ) {
                    spannable.setSpan(it,
                            start,
                            end,
                            Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                }
            }

            if (onTap != null) {
                val onTapSpan = OnTapSpan(TapContext(onTap, null))
                spannable.setSpan(onTapSpan,
                        start,
                        end,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
            }

            if (onLayout != null) {
                spannable.setSpan(OnLayoutSpan(onLayout, start, end - start),
                        start,
                        end,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
            }
        }

        return spannable
    }

    /**
     * Draws a Valdi's AttributedText object on top of an existing Android SpannedString
     */
    fun drawOnTop(
        canvas: Canvas,
        layout: Layout,
        overlayLayoutCache: OverlayLayoutCache,
        view: TextView,
        parsedAttributedText: ParsedAttributedText,
        missingFontsTracker: MissingFontsTracker
    ) {
        val fontAttributesArray = parsedAttributedText.attributes
        resolveLayoutOrigin(view, layout, layoutOrigin)
        val bitmapPaint = overlayBitmapPaint

        canvas.save()
        canvas.translate(layoutOrigin.x, layoutOrigin.y)
        for (partIndex in fontAttributesArray.indices) {
            val attributes = fontAttributesArray[partIndex]
            val animationTransform = attributes.animationTransform
            val partChunks = overlayLayoutCache.drawChunksByPartIndex[partIndex] ?: continue

            if (isRenderableAnimationTransform(animationTransform)) {
                val overlayAlpha = (animationTransform!!.opacity * 255).toInt().coerceIn(0, 255)
                bitmapPaint.alpha = overlayAlpha
                val isValdiEditText = view is com.snap.valdi.views.ValdiEditText
                val outlinePaint = if (isValdiEditText &&
                    attributes.outlineColor != null &&
                    attributes.outlineWidth > 0f
                ) {
                    attributes.toPaint(fontManager, missingFontsTracker)
                } else {
                    null
                }

                for (chunk in partChunks) {
                    val drawCenterX = if (shouldAdjustSpacing(animationTransform)) {
                        overlayLayoutCache.lineCenterByIndex[chunk.lineIndex] ?: chunk.drawBounds.centerX()
                    } else {
                        chunk.drawBounds.centerX()
                    }
                    val drawCenterY = chunk.drawBounds.centerY()

                    canvas.save()
                    canvas.translate(drawCenterX, drawCenterY + animationTransform.translationY)
                    canvas.scale(animationTransform.scale, animationTransform.scale)
                    canvas.translate(-drawCenterX, -drawCenterY)
                    canvas.drawBitmap(chunk.bitmap, chunk.bitmapLeft, chunk.bitmapTop, bitmapPaint)
                    if (outlinePaint != null) {
                        outlinePaint.alpha = overlayAlpha
                        canvas.drawText(chunk.chunkText, chunk.x, chunk.baselineY, outlinePaint)
                    }
                    canvas.restore()
                }
            } else if (!isActiveAnimationTransform(animationTransform)) {
                bitmapPaint.alpha = 255
                val isValdiEditText = view is com.snap.valdi.views.ValdiEditText
                val outlinePaint = if (isValdiEditText &&
                    attributes.outlineColor != null &&
                    attributes.outlineWidth > 0f
                ) {
                    attributes.toPaint(fontManager, missingFontsTracker)
                } else {
                    null
                }
                for (chunk in partChunks) {
                    if (!isValdiEditText) {
                        canvas.drawBitmap(chunk.bitmap, chunk.bitmapLeft, chunk.bitmapTop, bitmapPaint)
                    }
                    if (outlinePaint != null) {
                        canvas.drawText(chunk.chunkText, chunk.x, chunk.baselineY, outlinePaint)
                    }
                }
            }
        }
        canvas.restore()
    }

    fun drawOnTop(
        canvas: Canvas,
        layout: Layout,
        attributedText: AttributedText,
        startingAttributes: FontAttributes,
        missingFontsTracker: MissingFontsTracker
    ) {
        val partsSize = attributedText.getPartsSize()
        val contentLengths = parseContentLengths(attributedText)
        val fontAttributesArray = parseAttributes(attributedText, startingAttributes, contentLengths)
        var currentOffset = 0

        // we iterate over all font attributes/parts, such that for each line of drawn text, we attempt to overdraw as many as we can
        // thus, on a given line, we attempt to fill every character with the current chunk
        // we keep track of currentOffset to know the positioning of the next chunk within the line
        for (index in 0 until partsSize) {
            val attributes = fontAttributesArray[index]
            val chunkLength = contentLengths[index]
            var remainingChunkLength = chunkLength

            while (remainingChunkLength > 0) {
                val lineIndex = layout.getLineForOffset(currentOffset)
                val lineStart = layout.getLineStart(lineIndex)
                val lineEnd = layout.getLineEnd(lineIndex)
                // used to calculate the current text that's part of the current chunk, on the current line
                val chunkStartInLine = maxOf(currentOffset, lineStart)
                val chunkEndInLine = minOf(currentOffset + remainingChunkLength, lineEnd)
                val chunkText = layout.text.substring(chunkStartInLine, chunkEndInLine)
                val x = layout.getPrimaryHorizontal(chunkStartInLine)
                val y = layout.getLineBaseline(lineIndex).toFloat()

                // only overdraw for outlined chunks
                if (attributes.outlineColor != null && attributes.outlineWidth > 0f) {
                    val paint = attributes.toPaint(fontManager, missingFontsTracker)
                    canvas.drawText(chunkText, x, y, paint)
                }

                // for cases where chunks may overrun the current line
                val drawnChunkLength = chunkEndInLine - chunkStartInLine
                currentOffset += drawnChunkLength
                remainingChunkLength -= drawnChunkLength
            }
        }
    }

    fun buildOverlayLayoutCache(layout: Layout, view: TextView, overlaySpannable: Spannable): OverlayLayoutCache? {
        if (layout.width <= 0) {
            return null
        }

        val textDirection = TextViewUtils.resolveTextDirectionHeuristic(view)
        val builtLayout = buildStaticLayout(
            text = overlaySpannable,
            paint = TextPaint(view.paint),
            width = layout.width,
            alignment = layout.alignment,
            lineSpacingExtra = view.lineSpacingExtra,
            lineSpacingMultiplier = view.lineSpacingMultiplier,
            includeFontPadding = view.includeFontPadding,
            textDirection = textDirection,
            breakStrategy = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) view.breakStrategy else null,
            hyphenationFrequency = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) view.hyphenationFrequency else null,
            justificationMode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) view.justificationMode else null,
        )
        val bitmapPadding = kotlin.math.ceil(view.paint.textSize.toDouble()).toInt().coerceAtLeast(1)
        val drawChunks = buildOverlayDrawChunks(
            layout,
            builtLayout,
            overlaySpannable,
            view.includeFontPadding,
            bitmapPadding,
            textDirection,
        )
        val lineCenterByIndex = buildLineCenterByIndex(drawChunks)

        return OverlayLayoutCache(
            overlaySpannable = overlaySpannable,
            width = layout.width,
            alignment = layout.alignment,
            lineSpacingExtra = view.lineSpacingExtra,
            lineSpacingMultiplier = view.lineSpacingMultiplier,
            includeFontPadding = view.includeFontPadding,
            breakStrategy = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) view.breakStrategy else 0,
            hyphenationFrequency = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) view.hyphenationFrequency else 0,
            justificationMode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) view.justificationMode else 0,
            textDirection = textDirection,
            drawChunks = drawChunks,
            drawChunksByPartIndex = drawChunks.groupBy { it.partIndex },
            lineCenterByIndex = lineCenterByIndex,
        )
    }

    private fun buildLineCenterByIndex(drawChunks: List<OverlayDrawChunk>): Map<Int, Float> {
        val lineLeftMap = HashMap<Int, Float>()
        val lineRightMap = HashMap<Int, Float>()
        for (chunk in drawChunks) {
            val currentLeft = lineLeftMap[chunk.lineIndex]
            val currentRight = lineRightMap[chunk.lineIndex]
            lineLeftMap[chunk.lineIndex] = if (currentLeft == null) chunk.drawBounds.left else minOf(currentLeft, chunk.drawBounds.left)
            lineRightMap[chunk.lineIndex] = if (currentRight == null) chunk.drawBounds.right else maxOf(currentRight, chunk.drawBounds.right)
        }

        return lineLeftMap.mapValues { (lineIndex, left) ->
            val right = lineRightMap[lineIndex] ?: left
            (left + right) / 2f
        }
    }

    private fun buildOverlayDrawChunks(
        baseLayout: Layout,
        overlayLayout: StaticLayout,
        overlaySpannable: Spannable,
        includeFontPadding: Boolean,
        bitmapPadding: Int,
        textDirection: TextDirectionHeuristic,
    ): List<OverlayDrawChunk> {
        val chunks = mutableListOf<OverlayDrawChunk>()
        val text = overlaySpannable.toString()

        overlaySpannable.getSpans(0, overlaySpannable.length, PartIndexSpan::class.java).forEach { span ->
            val partIndex = span.partIndex
            var currentOffset = overlaySpannable.getSpanStart(span)
            val partEnd = overlaySpannable.getSpanEnd(span)

            while (currentOffset < partEnd) {
                val lineIndex = baseLayout.getLineForOffset(currentOffset)
                val lineStart = baseLayout.getLineStart(lineIndex)
                val lineEnd = baseLayout.getLineEnd(lineIndex)
                val lineTextEnd = if (lineEnd > lineStart && text[lineEnd - 1] == '\n') lineEnd - 1 else lineEnd
                val chunkStartInLine = maxOf(currentOffset, lineStart)
                val chunkEndInLine = minOf(partEnd, lineEnd)
                val drawEndInLine = minOf(chunkEndInLine, lineTextEnd)
                if (drawEndInLine <= chunkStartInLine) {
                    currentOffset = chunkEndInLine
                    continue
                }

                val chunkText = text.substring(chunkStartInLine, drawEndInLine)
                val x = baseLayout.getPrimaryHorizontal(chunkStartInLine)
                val baselineY = baseLayout.getLineBaseline(lineIndex).toFloat()

                val chunkSpannable = SpannableStringBuilder(chunkText)
                TextUtils.copySpansFrom(overlaySpannable, chunkStartInLine, drawEndInLine, null, chunkSpannable, 0)
                chunkSpannable.getSpans(0, chunkSpannable.length, PartIndexSpan::class.java).forEach {
                    chunkSpannable.removeSpan(it)
                }
                chunkSpannable.getSpans(0, chunkSpannable.length, AlignmentSpan::class.java).forEach {
                    chunkSpannable.removeSpan(it)
                }
                val chunkTextPaint = TextPaint(overlayLayout.paint)
                val desiredChunkWidth = Layout.getDesiredWidth(chunkSpannable, chunkTextPaint)
                val endOffsetStaysOnSameLine =
                    drawEndInLine >= text.length || baseLayout.getLineForOffset(drawEndInLine) == lineIndex
                val measuredChunkWidth = if (endOffsetStaysOnSameLine) {
                    kotlin.math.abs(baseLayout.getPrimaryHorizontal(drawEndInLine) - x)
                } else {
                    0f
                }
                val chunkLayoutWidth =
                    kotlin.math.ceil(maxOf(measuredChunkWidth, desiredChunkWidth).toDouble()).toInt().coerceAtLeast(1)
                val chunkLayout = buildStaticLayout(
                    text = chunkSpannable,
                    paint = chunkTextPaint,
                    width = chunkLayoutWidth,
                    alignment = Layout.Alignment.ALIGN_NORMAL,
                    lineSpacingExtra = 0f,
                    lineSpacingMultiplier = 1f,
                    includeFontPadding = includeFontPadding,
                    textDirection = textDirection,
                    maxLines = 1,
                )
                val runIsRtl = baseLayout.isRtlCharAt(chunkStartInLine)
                val chunkOriginX = if (runIsRtl) {
                    x - chunkLayout.width
                } else {
                    x
                }
                val drawBounds = RectF(
                    chunkOriginX + chunkLayout.getLineLeft(0),
                    baseLayout.getLineTop(lineIndex).toFloat(),
                    chunkOriginX + chunkLayout.getLineRight(0),
                    baseLayout.getLineBottom(lineIndex).toFloat(),
                )

                val chunkBitmapWidth = (chunkLayout.width + bitmapPadding * 2).coerceAtLeast(1)
                val chunkBitmapHeight = (chunkLayout.height + bitmapPadding * 2).coerceAtLeast(1)
                val chunkBitmap = Bitmap.createBitmap(chunkBitmapWidth, chunkBitmapHeight, Bitmap.Config.ARGB_8888)
                Canvas(chunkBitmap).apply {
                    translate(bitmapPadding.toFloat(), bitmapPadding.toFloat())
                    chunkLayout.draw(this)
                }
                val chunkBaselineY = chunkLayout.getLineBaseline(0).toFloat()

                chunks.add(
                    OverlayDrawChunk(
                        partIndex = partIndex,
                        startOffset = chunkStartInLine,
                        chunkText = chunkText,
                        x = x,
                        baselineY = baselineY,
                        lineIndex = lineIndex,
                        drawBounds = drawBounds,
                        bitmap = chunkBitmap,
                        bitmapLeft = chunkOriginX - bitmapPadding.toFloat(),
                        bitmapTop = baselineY - chunkBaselineY - bitmapPadding.toFloat(),
                    )
                )

                currentOffset = chunkEndInLine
            }
        }

        chunks.sortWith(
            compareBy<OverlayDrawChunk> { it.lineIndex }
                .thenBy { it.startOffset }
                .thenBy { it.partIndex },
        )

        return chunks
    }

    private fun buildStaticLayout(
        text: CharSequence,
        paint: TextPaint,
        width: Int,
        alignment: Layout.Alignment,
        lineSpacingExtra: Float,
        lineSpacingMultiplier: Float,
        includeFontPadding: Boolean,
        textDirection: TextDirectionHeuristic,
        breakStrategy: Int? = null,
        hyphenationFrequency: Int? = null,
        justificationMode: Int? = null,
        maxLines: Int? = null,
    ): StaticLayout {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val builder = StaticLayout.Builder.obtain(text, 0, text.length, paint, width)
                .setAlignment(alignment)
                .setLineSpacing(lineSpacingExtra, lineSpacingMultiplier)
                .setIncludePad(includeFontPadding)
                .setTextDirection(textDirection)

            breakStrategy?.let(builder::setBreakStrategy)
            hyphenationFrequency?.let(builder::setHyphenationFrequency)
            maxLines?.let(builder::setMaxLines)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                justificationMode?.let(builder::setJustificationMode)
            }
            return builder.build()
        }

        @Suppress("DEPRECATION")
        return StaticLayout(
            text,
            0,
            text.length,
            paint,
            width,
            alignment,
            lineSpacingMultiplier,
            lineSpacingExtra,
            includeFontPadding,
        )
    }

    private fun resolveLayoutOrigin(view: TextView, layout: Layout, outOrigin: PointF) {
        val layoutHeight = layout.height
        val availableHeight = view.height - view.extendedPaddingTop - view.extendedPaddingBottom

        outOrigin.x = view.totalPaddingLeft.toFloat()
        var dy = view.extendedPaddingTop.toFloat()

        when (view.gravity and Gravity.VERTICAL_GRAVITY_MASK) {
            Gravity.CENTER_VERTICAL -> dy += ((availableHeight - layoutHeight).coerceAtLeast(0)) / 2f
            Gravity.BOTTOM -> dy += (availableHeight - layoutHeight).coerceAtLeast(0).toFloat()
        }

        outOrigin.y = dy
    }

    private fun shouldAdjustSpacing(animationTransform: TextAnimationTransform?): Boolean {
        if (animationTransform == null) {
            return false
        }
        return kotlin.math.abs(animationTransform.translationY) < 0.01f &&
            kotlin.math.abs(animationTransform.scale - 1f) > 0.01f
    }
}
