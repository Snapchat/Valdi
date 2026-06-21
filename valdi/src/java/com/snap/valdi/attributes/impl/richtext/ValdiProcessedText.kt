package com.snap.valdi.attributes.impl.richtext

import android.graphics.Canvas
import android.text.Layout
import android.text.Spannable
import android.text.SpannableStringBuilder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.ValdiMarshaller
import java.util.concurrent.ConcurrentHashMap
import java.util.regex.Pattern
import java.util.regex.PatternSyntaxException

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
        private val compiledPartPatterns = ConcurrentHashMap<String, Pattern>()

        fun parse(
            fontManager: FontManager,
            attributedText: AttributedText,
            startingAttributes: FontAttributes,
            missingFontsTracker: MissingFontsTracker,
            logger: Logger,
            attributedTextAnimator: AttributedTextAnimator? = null,
            disableTextReplacement: Boolean = false,
            density: Float = 1.0f
        ): ValdiProcessedText {
            val sourcePartCount = attributedText.getPartsSize()
            val attributes = parseAttributes(attributedText, startingAttributes)
            val spannable = SpannableStringBuilder()
            var onTapCallbacks: MutableList<RangeValue<ValdiFunction>>? = null
            var onLayoutCallbacks: MutableList<RangeValue<ValdiFunction>>? = null
            var inlineViewAttachments: MutableList<RangeValue<InlineViewAttachmentSpan>>? = null
            var inlineViewAttachmentPartIndexes: MutableList<Int>? = null
            var inlineViewAttachmentsByChildIndex: MutableList<RangeValue<InlineViewAttachmentSpan>?>? = null
            var animationTransforms: MutableList<RangeValue<AttributedTextAnimation>>? = null
            var drawOnTopItems: MutableList<RangeValue<DrawOnTopValue>>? = null
            var animationPartIndexesByGroup: MutableList<Int>? = null
            var nextSyntheticAnimationPartIndex = sourcePartCount

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
                var animationRange: RangeValue<AttributedTextAnimation>? = null
                var splitAnimationRanges: MutableList<RangeValue<AttributedTextAnimation>>? = null
                if (attributedTextAnimator != null &&
                    imageAttachment == null &&
                    animationTransform != null
                ) {
                    nextSyntheticAnimationPartIndex = forAnimationParts(
                        animationTransform,
                        content,
                        start,
                        end,
                        index,
                        inlineViewAttachment != null,
                        nextSyntheticAnimationPartIndex,
                        logger
                    ) { partStart, partEnd, partIndex, isSynthetic ->
                        val newAnimationRange = animationRangeForPart(
                            attributedTextAnimator,
                            animationTransform,
                            partStart,
                            partEnd,
                            partIndex,
                            animationPartIndexesByGroup
                        ) {
                            mutableListOf<Int>().also {
                                animationPartIndexesByGroup = it
                            }
                        }
                        if (newAnimationRange != null) {
                            if (isSynthetic) {
                                splitAnimationRanges = appendValue(splitAnimationRanges, newAnimationRange)
                            } else {
                                animationRange = newAnimationRange
                            }
                        }
                    }
                }
                val hasAnimationRanges = animationRange != null || splitAnimationRanges != null
                val useAnimatedReplacementSpan = hasAnimationRanges && inlineViewAttachment == null
                val usesAttachmentReplacementSpan = inlineViewAttachment != null || (imageAttachment != null && !disableTextReplacement)
                val disableAttributeTextReplacement = disableTextReplacement || useAnimatedReplacementSpan || usesAttachmentReplacementSpan
                val hasAnimatedDrawableUnderline = hasAnimationRanges && attribute.requiresDrawableUnderlineSpan()

                attribute.enumerateSpans(
                    fontManager,
                    missingFontsTracker,
                    disableAttributeTextReplacement,
                    !hasAnimatedDrawableUnderline
                ) {
                    spannable.setSpan(it, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                }
                if (hasAnimatedDrawableUnderline) {
                    appendDrawableUnderlineSpans(
                        spannable,
                        attribute,
                        start,
                        end,
                        animationRange,
                        splitAnimationRanges,
                        includeAnimatedRanges = !useAnimatedReplacementSpan
                    )
                }

                if (inlineViewAttachment != null) {
                    val span = InlineViewAttachmentSpan(inlineViewAttachment, density, animationRange?.value)
                    spannable.setSpan(span, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                    spannable.append(THIN_SPACE_FOR_LINE_BREAK)
                    val rangeValue = RangeValue(start, end, span)
                    inlineViewAttachments = appendValue(inlineViewAttachments, rangeValue)
                    inlineViewAttachmentPartIndexes = appendValue(inlineViewAttachmentPartIndexes, index)
                    val childIndex = inlineViewAttachment.childIndex
                    if (childIndex >= 0) {
                        inlineViewAttachmentsByChildIndex =
                            setValueAtIndex(inlineViewAttachmentsByChildIndex, childIndex, rangeValue)
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
                    onTapCallbacks = appendValue(onTapCallbacks, RangeValue(start, end, onTap))
                }

                if (onLayout != null) {
                    onLayoutCallbacks = appendValue(onLayoutCallbacks, RangeValue(start, end, onLayout))
                }

                if (hasAnimationRanges) {
                    forAnimationRanges(animationRange, splitAnimationRanges) { range ->
                        animationTransforms = appendAnimationRange(
                            animationTransforms,
                            spannable,
                            range,
                            useAnimatedReplacementSpan,
                            attribute,
                            fontManager,
                            missingFontsTracker,
                            density
                        )
                    }
                }

                if (attribute.outlineColor != null && attribute.outlineWidth > 0f) {
                    if (!hasAnimationRanges) {
                        drawOnTopItems = appendDrawOnTopRange(drawOnTopItems, start, end, attribute)
                    } else if (useAnimatedReplacementSpan) {
                        drawOnTopItems = addUnanimatedOutlineRanges(
                            drawOnTopItems,
                            start,
                            end,
                            animationRange,
                            splitAnimationRanges,
                            attribute
                        )
                    }
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

        private fun <T> appendValue(items: MutableList<T>?, value: T): MutableList<T> {
            val output = items ?: mutableListOf()
            output.add(value)
            return output
        }

        private fun <T> setValueAtIndex(items: MutableList<T?>?, index: Int, value: T): MutableList<T?> {
            val output = items ?: mutableListOf()
            while (output.size <= index) {
                output.add(null)
            }
            output[index] = value
            return output
        }

        private fun appendAnimationRange(
            animationTransforms: MutableList<RangeValue<AttributedTextAnimation>>?,
            spannable: SpannableStringBuilder,
            animationRange: RangeValue<AttributedTextAnimation>,
            useAnimatedReplacementSpan: Boolean,
            attribute: FontAttributes,
            fontManager: FontManager,
            missingFontsTracker: MissingFontsTracker,
            density: Float
        ): MutableList<RangeValue<AttributedTextAnimation>> {
            if (useAnimatedReplacementSpan) {
                spannable.setSpan(
                    AnimatedTextReplacementSpan(animationRange.value, attribute, fontManager, missingFontsTracker, density),
                    animationRange.start,
                    animationRange.end,
                    Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
                )
            }
            return appendValue(animationTransforms, animationRange)
        }

        private fun appendDrawableUnderlineSpans(
            spannable: SpannableStringBuilder,
            attribute: FontAttributes,
            start: Int,
            end: Int,
            animationRange: RangeValue<AttributedTextAnimation>?,
            splitAnimationRanges: List<RangeValue<AttributedTextAnimation>>?,
            includeAnimatedRanges: Boolean
        ) {
            if (animationRange != null) {
                val rangeEnd = appendDrawableUnderlineSpans(spannable, attribute, start, animationRange, includeAnimatedRanges)
                appendDrawableUnderlineSpanIfNotEmpty(spannable, attribute, rangeEnd, end, null)
                return
            }

            if (splitAnimationRanges == null) {
                return
            }

            var rangeStart = start
            for (splitAnimationRange in splitAnimationRanges) {
                rangeStart = appendDrawableUnderlineSpans(
                    spannable,
                    attribute,
                    rangeStart,
                    splitAnimationRange,
                    includeAnimatedRanges
                )
            }
            appendDrawableUnderlineSpanIfNotEmpty(spannable, attribute, rangeStart, end, null)
        }

        private fun appendDrawableUnderlineSpans(
            spannable: SpannableStringBuilder,
            attribute: FontAttributes,
            rangeStart: Int,
            animationRange: RangeValue<AttributedTextAnimation>,
            includeAnimatedRange: Boolean
        ): Int {
            appendDrawableUnderlineSpanIfNotEmpty(spannable, attribute, rangeStart, animationRange.start, null)
            if (includeAnimatedRange) {
                appendDrawableUnderlineSpanIfNotEmpty(
                    spannable,
                    attribute,
                    animationRange.start,
                    animationRange.end,
                    animationRange.value
                )
            }
            return animationRange.end
        }

        private fun appendDrawableUnderlineSpanIfNotEmpty(
            spannable: SpannableStringBuilder,
            attribute: FontAttributes,
            start: Int,
            end: Int,
            animation: AttributedTextAnimation?
        ) {
            if (start >= end) {
                return
            }

            val span = attribute.createDrawableUnderlineSpan(animation) ?: return
            spannable.setSpan(span, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        }

        private inline fun forAnimationRanges(
            animationRange: RangeValue<AttributedTextAnimation>?,
            splitAnimationRanges: List<RangeValue<AttributedTextAnimation>>?,
            block: (RangeValue<AttributedTextAnimation>) -> Unit
        ) {
            if (animationRange != null) {
                block(animationRange)
            }
            if (splitAnimationRanges != null) {
                for (splitAnimationRange in splitAnimationRanges) {
                    block(splitAnimationRange)
                }
            }
        }

        private fun forAnimationParts(
            animationTransform: TextAnimationTransform,
            content: String,
            partStart: Int,
            partEnd: Int,
            partIndex: Int,
            isAttachmentPart: Boolean,
            nextSyntheticPartIndex: Int,
            logger: Logger,
            onPart: (Int, Int, Int, Boolean) -> Unit
        ): Int {
            val partPattern = animationTransform.partPattern
            if (isAttachmentPart || partPattern.isNullOrEmpty()) {
                onPart(partStart, partEnd, partIndex, false)
                return nextSyntheticPartIndex
            }

            val pattern = compiledPartPattern(partPattern, logger) ?: return nextSyntheticPartIndex
            var nextPartIndex = nextSyntheticPartIndex
            val matcher = pattern.matcher(content)
            while (matcher.find()) {
                val matchStart = matcher.start()
                val matchEnd = matcher.end()
                if (matchEnd > matchStart) {
                    onPart(partStart + matchStart, partStart + matchEnd, nextPartIndex++, true)
                }
            }
            return nextPartIndex
        }

        private fun compiledPartPattern(partPattern: String, logger: Logger): Pattern? {
            val cachedPattern = compiledPartPatterns[partPattern]
            if (cachedPattern != null) {
                return cachedPattern
            }

            val compiledPattern = try {
                Pattern.compile(partPattern)
            } catch (exc: PatternSyntaxException) {
                logger.log(
                    LogLevel.ERROR,
                    exc,
                    "Invalid text animation partPattern: $partPattern"
                )
                return null
            }

             // Prevent unbounded growth if partPattern values are dynamic.
             if (compiledPartPatterns.size > 64) {
                 compiledPartPatterns.clear()
             }

            return compiledPartPatterns.putIfAbsent(partPattern, compiledPattern) ?: compiledPattern
        }

        private inline fun animationRangeForPart(
            attributedTextAnimator: AttributedTextAnimator,
            animationTransform: TextAnimationTransform,
            start: Int,
            end: Int,
            partIndex: Int,
            partIndexesByGroup: MutableList<Int>?,
            createPartIndexesByGroup: () -> MutableList<Int>
        ): RangeValue<AttributedTextAnimation>? {
            val groupIndex = animationTransform.groupIndex
            val partIndexInGroup = partIndexesByGroup?.getOrNull(groupIndex) ?: 0
            val emittedTransform = textAnimationTransformWithPartIndexInGroup(
                animationTransform,
                partIndexInGroup
            )
            val animation = attributedTextAnimator.animationForPart(partIndex, emittedTransform, start, end) ?: return null
            val mutablePartIndexesByGroup = partIndexesByGroup ?: createPartIndexesByGroup()
            while (mutablePartIndexesByGroup.size <= groupIndex) {
                mutablePartIndexesByGroup.add(0)
            }
            mutablePartIndexesByGroup[groupIndex] = partIndexInGroup + 1
            return RangeValue(start, end, animation)
        }

        private fun textAnimationTransformWithPartIndexInGroup(
            animationTransform: TextAnimationTransform,
            partIndexInGroup: Int
        ): TextAnimationTransform {
            return TextAnimationTransform(
                key = animationTransform.key,
                translationY = animationTransform.translationY,
                scale = animationTransform.scale,
                opacity = animationTransform.opacity,
                duration = animationTransform.duration,
                timeOffsetBetweenParts = animationTransform.timeOffsetBetweenParts,
                groupIndex = animationTransform.groupIndex,
                partIndexInGroup = partIndexInGroup,
                partPattern = animationTransform.partPattern
            )
        }

        private fun addUnanimatedOutlineRanges(
            drawOnTopItems: MutableList<RangeValue<DrawOnTopValue>>?,
            start: Int,
            end: Int,
            animationRange: RangeValue<AttributedTextAnimation>?,
            splitAnimationRanges: List<RangeValue<AttributedTextAnimation>>?,
            attribute: FontAttributes
        ): MutableList<RangeValue<DrawOnTopValue>>? {
            return if (animationRange != null) {
                addUnanimatedOutlineRanges(drawOnTopItems, start, end, animationRange, attribute)
            } else if (splitAnimationRanges != null) {
                addUnanimatedOutlineRanges(drawOnTopItems, start, end, splitAnimationRanges, attribute)
            } else {
                drawOnTopItems
            }
        }

        private fun addUnanimatedOutlineRanges(
            drawOnTopItems: MutableList<RangeValue<DrawOnTopValue>>?,
            start: Int,
            end: Int,
            animationRange: RangeValue<AttributedTextAnimation>,
            attribute: FontAttributes
        ): MutableList<RangeValue<DrawOnTopValue>>? {
            var items = drawOnTopItems
            items = appendDrawOnTopRangeIfNotEmpty(items, start, animationRange.start, attribute)
            items = appendDrawOnTopRangeIfNotEmpty(items, animationRange.end, end, attribute)
            return items
        }

        private fun addUnanimatedOutlineRanges(
            drawOnTopItems: MutableList<RangeValue<DrawOnTopValue>>?,
            start: Int,
            end: Int,
            animationRanges: List<RangeValue<AttributedTextAnimation>>,
            attribute: FontAttributes
        ): MutableList<RangeValue<DrawOnTopValue>>? {
            var items = drawOnTopItems
            var rangeStart = start
            for (animationRange in animationRanges) {
                items = appendDrawOnTopRangeIfNotEmpty(items, rangeStart, animationRange.start, attribute)
                rangeStart = animationRange.end
            }
            items = appendDrawOnTopRangeIfNotEmpty(items, rangeStart, end, attribute)
            return items
        }

        private fun appendDrawOnTopRangeIfNotEmpty(
            drawOnTopItems: MutableList<RangeValue<DrawOnTopValue>>?,
            start: Int,
            end: Int,
            attribute: FontAttributes
        ): MutableList<RangeValue<DrawOnTopValue>>? {
            return if (start < end) {
                appendDrawOnTopRange(drawOnTopItems, start, end, attribute)
            } else {
                drawOnTopItems
            }
        }

        private fun appendDrawOnTopRange(
            drawOnTopItems: MutableList<RangeValue<DrawOnTopValue>>?,
            start: Int,
            end: Int,
            attribute: FontAttributes
        ): MutableList<RangeValue<DrawOnTopValue>> {
            return appendValue(drawOnTopItems, RangeValue(start, end, DrawOnTopValue(attribute, null)))
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
