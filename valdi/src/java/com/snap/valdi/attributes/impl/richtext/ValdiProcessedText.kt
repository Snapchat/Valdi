package com.snap.valdi.attributes.impl.richtext

import android.graphics.Canvas
import android.text.Layout
import android.text.Spannable
import android.text.SpannableStringBuilder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.ValdiMarshaller

/**
 * Parsed, platform-ready representation of Valdi attributed text on Android.
 *
 * This class owns the mutable [SpannableStringBuilder] installed on the backing
 * TextView and indexes all Valdi-specific metadata that used to be recovered by
 * scanning spans: callbacks, inline child attachments, text animations, and
 * draw-on-top effects. Keeping those side tables here makes per-layout queries
 * cheap and gives inline attachments a single place to refresh their measured
 * size without reparsing the original attributed-text value.
 */
class ValdiProcessedText private constructor(
    private val attributedText: AttributedText,
    val spannable: SpannableStringBuilder,
    val onTapCallbacks: List<RangeValue<ValdiFunction>>?,
    val onLayoutCallbacks: List<RangeValue<ValdiFunction>>?,
    val inlineViewAttachments: List<RangeValue<InlineViewAttachmentSpan>>?,
    private val inlineViewAttachmentPartIndexes: List<Int>?,
    private val inlineViewAttachmentsByChildIndex: List<RangeValue<InlineViewAttachmentSpan>?>?,
    val animationTransforms: List<RangeValue<AttributedTextAnimation>>?,
    private val drawOnTopItems: List<RangeValue<DrawOnTopValue>>?
) {
    /**
     * Associates a parsed attributed-text value with the character range it
     * occupies in [spannable].
     */
    data class RangeValue<T>(
        val start: Int,
        val end: Int,
        val value: T
    ) {
        val length: Int
            get() = end - start

        fun contains(index: Int): Boolean {
            return start <= index && index < end
        }
    }

    /**
     * Draw-on-top metadata for text effects that Android TextView cannot render
     * directly from normal spans.
     */
    private data class DrawOnTopValue(
        val attributes: FontAttributes,
        val animation: AttributedTextAnimation?
    )

    val hasOnTap: Boolean
        get() = onTapCallbacks != null

    val hasOnLayout: Boolean
        get() = onLayoutCallbacks != null

    val hasInlineViewAttachment: Boolean
        get() = inlineViewAttachments != null

    val hasAnimationTransform: Boolean
        get() = animationTransforms != null

    val hasOuterOutline: Boolean
        get() = drawOnTopItems != null

    val animationTransformsCount: Int
        get() = animationTransforms?.size ?: 0

    fun onTapAtIndex(index: Int): RangeValue<ValdiFunction>? {
        return onTapCallbacks?.firstOrNull { it.contains(index) }
    }

    fun onLayoutAtIndex(index: Int): RangeValue<ValdiFunction>? {
        return onLayoutCallbacks?.firstOrNull { it.contains(index) }
    }

    fun inlineViewAttachmentAtIndex(index: Int): RangeValue<InlineViewAttachmentSpan>? {
        return inlineViewAttachments?.firstOrNull { it.contains(index) }
    }

    fun inlineViewAttachmentForViewIndex(childIndex: Int): RangeValue<InlineViewAttachmentSpan>? {
        val inlineViewAttachmentsByChildIndex = inlineViewAttachmentsByChildIndex ?: return null
        return if (childIndex >= 0 && childIndex < inlineViewAttachmentsByChildIndex.size) {
            inlineViewAttachmentsByChildIndex[childIndex]
        } else {
            null
        }
    }

    fun hasInlineViewAttachmentForIndex(childIndex: Int): Boolean {
        return inlineViewAttachmentForViewIndex(childIndex) != null
    }

    fun updateInlineAttachments(): Boolean {
        val inlineViewAttachments = inlineViewAttachments ?: return false
        val inlineViewAttachmentPartIndexes = inlineViewAttachmentPartIndexes ?: return false
        var didChange = false
        for ((index, item) in inlineViewAttachments.withIndex()) {
            val partIndex = inlineViewAttachmentPartIndexes[index]
            val attachmentInfo = attributedText.getInlineViewAttachmentAtIndex(partIndex) ?: continue
            if (item.value.updateAttachmentInfo(attachmentInfo)) {
                didChange = true
            }
        }
        return didChange
    }

    fun drawOnTop(
        canvas: Canvas,
        layout: Layout,
        fontManager: FontManager,
        missingFontsTracker: MissingFontsTracker
    ) {
        val drawOnTopItems = drawOnTopItems ?: return
        var currentOffset = 0

        for (item in drawOnTopItems) {
            val attributes = item.value.attributes
            val animation = item.value.animation
            var remainingChunkLength = item.length
            currentOffset = item.start

            while (remainingChunkLength > 0) {
                val lineIndex = layout.getLineForOffset(currentOffset)
                val lineStart = layout.getLineStart(lineIndex)
                val lineEnd = layout.getLineEnd(lineIndex)

                val chunkStartInLine = Math.max(currentOffset, lineStart)
                val chunkEndInLine = Math.min(currentOffset + remainingChunkLength, lineEnd)
                val chunkText = layout.text.substring(chunkStartInLine, chunkEndInLine)
                val x = layout.getPrimaryHorizontal(chunkStartInLine)
                val y = layout.getLineBaseline(lineIndex).toFloat()

                if (animation == null && attributes.outlineColor != null && attributes.outlineWidth > 0f) {
                    val paint = attributes.toPaint(fontManager, missingFontsTracker)
                    canvas.drawText(chunkText, x, y, paint)
                }

                val drawnChunkLength = chunkEndInLine - chunkStartInLine
                currentOffset += drawnChunkLength
                remainingChunkLength -= drawnChunkLength
            }
        }
    }

    fun updateOnLayoutCallbacks(layout: Layout, coordinateResolver: CoordinateResolver) {
        val onLayoutCallbacks = onLayoutCallbacks ?: return
        for (item in onLayoutCallbacks) {
            if (item.start < 0 || item.end < 0 || item.start > item.end || item.end > spannable.length) {
                continue
            }

            val lineStart = layout.getLineForOffset(item.start)
            val xStart = coordinateResolver.fromPixel(layout.getPrimaryHorizontal(item.start))
            val yStart = coordinateResolver.fromPixel(layout.getLineTop(lineStart).toDouble())

            val lineEnd = layout.getLineForOffset(item.end)
            val xEnd = coordinateResolver.fromPixel(layout.getPrimaryHorizontal(item.end))
            val yEnd = coordinateResolver.fromPixel(layout.getLineBottom(lineEnd).toDouble())

            performOnLayoutCallback(
                item.value,
                xStart.toDouble(),
                yStart.toDouble(),
                (xEnd - xStart).toDouble(),
                (yEnd - yStart).toDouble()
            )
        }
    }

    private fun performOnLayoutCallback(function: ValdiFunction, x: Double, y: Double, width: Double, height: Double) {
        ValdiMarshaller.use {
            it.pushDouble(x)
            it.pushDouble(y)
            it.pushDouble(width)
            it.pushDouble(height)
            function.perform(it)
        }
    }

    companion object {
        fun parse(
            fontManager: FontManager,
            attributedText: AttributedText,
            startingAttributes: FontAttributes,
            missingFontsTracker: MissingFontsTracker,
            attributedTextAnimator: AttributedTextAnimator? = null,
            disableTextReplacement: Boolean = false,
            density: Float = 1.0f
        ): ValdiProcessedText {
            val attributes = parseAttributes(attributedText, startingAttributes)
            val spannable = SpannableStringBuilder()
            var onTapCallbacks: MutableList<RangeValue<ValdiFunction>>? = null
            var onLayoutCallbacks: MutableList<RangeValue<ValdiFunction>>? = null
            var inlineViewAttachments: MutableList<RangeValue<InlineViewAttachmentSpan>>? = null
            var inlineViewAttachmentPartIndexes: MutableList<Int>? = null
            var inlineViewAttachmentsByChildIndex: MutableList<RangeValue<InlineViewAttachmentSpan>?>? = null
            var animationTransforms: MutableList<RangeValue<AttributedTextAnimation>>? = null
            var drawOnTopItems: MutableList<RangeValue<DrawOnTopValue>>? = null

            for ((index, attribute) in attributes.withIndex()) {
                val content = attributedText.getContentAtIndex(index)
                val start = spannable.length
                spannable.append(content)
                val end = spannable.length

                val onTap = attributedText.getOnTapAtIndex(index)
                val onLayout = attributedText.getOnLayoutAtIndex(index)
                val imageAttachment = attributedText.getImageAttachmentAtIndex(index)
                val inlineViewAttachment = attributedText.getInlineViewAttachmentAtIndex(index)
                val animationTransform = attributedText.getAnimationTransformAtIndex(index)
                val animation = if (attributedTextAnimator != null &&
                    imageAttachment == null &&
                    animationTransform != null
                ) {
                    attributedTextAnimator.animationForPart(index, animationTransform, start, end)
                } else {
                    null
                }
                val useAnimatedReplacementSpan = animation != null && inlineViewAttachment == null
                val usesAttachmentReplacementSpan = inlineViewAttachment != null || (imageAttachment != null && !disableTextReplacement)
                val disableAttributeTextReplacement = disableTextReplacement || useAnimatedReplacementSpan || usesAttachmentReplacementSpan

                attribute.enumerateSpans(fontManager, missingFontsTracker, disableAttributeTextReplacement) {
                    spannable.setSpan(it, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                }

                if (inlineViewAttachment != null) {
                    val span = InlineViewAttachmentSpan(inlineViewAttachment, density, animation)
                    spannable.setSpan(span, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                    spannable.append(THIN_SPACE_FOR_LINE_BREAK)
                    if (inlineViewAttachments == null) {
                        inlineViewAttachments = mutableListOf()
                    }
                    if (inlineViewAttachmentPartIndexes == null) {
                        inlineViewAttachmentPartIndexes = mutableListOf()
                    }
                    val rangeValue = RangeValue(start, end, span)
                    inlineViewAttachments.add(rangeValue)
                    inlineViewAttachmentPartIndexes.add(index)
                    val childIndex = inlineViewAttachment.childIndex
                    if (childIndex >= 0) {
                        if (inlineViewAttachmentsByChildIndex == null) {
                            inlineViewAttachmentsByChildIndex = mutableListOf()
                        }
                        while (inlineViewAttachmentsByChildIndex.size <= childIndex) {
                            inlineViewAttachmentsByChildIndex.add(null)
                        }
                        inlineViewAttachmentsByChildIndex[childIndex] = rangeValue
                    }
                } else if (imageAttachment != null && !disableTextReplacement) {
                    spannable.setSpan(
                        ImageAttachmentSpan(imageAttachment, density),
                        start,
                        end,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
                    )
                    spannable.append(THIN_SPACE_FOR_LINE_BREAK)
                }

                if (onTap != null) {
                    if (onTapCallbacks == null) {
                        onTapCallbacks = mutableListOf()
                    }
                    onTapCallbacks.add(RangeValue(start, end, onTap))
                }

                if (onLayout != null) {
                    if (onLayoutCallbacks == null) {
                        onLayoutCallbacks = mutableListOf()
                    }
                    onLayoutCallbacks.add(RangeValue(start, end, onLayout))
                }

                if (animation != null) {
                    if (useAnimatedReplacementSpan) {
                        spannable.setSpan(
                            AnimatedTextReplacementSpan(animation, attribute, fontManager, missingFontsTracker, density),
                            start,
                            end,
                            Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
                        )
                    }
                    if (animationTransforms == null) {
                        animationTransforms = mutableListOf()
                    }
                    animationTransforms.add(RangeValue(start, end, animation))
                }

                if (animation == null && attribute.outlineColor != null && attribute.outlineWidth > 0f) {
                    if (drawOnTopItems == null) {
                        drawOnTopItems = mutableListOf()
                    }
                    drawOnTopItems.add(RangeValue(start, end, DrawOnTopValue(attribute, animation)))
                }
            }

            return ValdiProcessedText(
                attributedText,
                spannable,
                onTapCallbacks,
                onLayoutCallbacks,
                inlineViewAttachments,
                inlineViewAttachmentPartIndexes,
                inlineViewAttachmentsByChildIndex,
                animationTransforms,
                drawOnTopItems
            )
        }

        private fun parseAttributes(attributedText: AttributedText, startingAttributes: FontAttributes): Array<FontAttributes> {
            val partsSize = attributedText.getPartsSize()
            return Array(partsSize) { index ->
                val attributes = startingAttributes.copy()
                val font = attributedText.getFontAtIndex(index)
                if (font != null) {
                    attributes.applyFont(font)
                }

                val color = attributedText.getColorAtIndex(index)
                if (color != null) {
                    attributes.color = color
                }

                val backgroundColor = attributedText.getBackgroundColorAtIndex(index)
                if (backgroundColor != null) {
                    attributes.backgroundColor = backgroundColor
                }

                val outlineColor = attributedText.getOutlineColorAtIndex(index)
                if (outlineColor != null) {
                    attributes.outlineColor = outlineColor
                }
                attributes.outlineWidth = attributedText.getOutlineWidthAtIndex(index)

                val textDecoration = attributedText.getTextDecorationAtIndex(index)
                if (textDecoration != null) {
                    attributes.textDecoration = textDecoration
                }

                attributes
            }
        }

        private const val THIN_SPACE_FOR_LINE_BREAK = "\u2009"
    }
}
