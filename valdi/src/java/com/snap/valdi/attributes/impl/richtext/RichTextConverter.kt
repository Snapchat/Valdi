package com.snap.valdi.attributes.impl.richtext

import android.graphics.Canvas
import android.text.Layout
import android.text.Spannable
import android.text.SpannableStringBuilder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.gestures.TapContext

class RichTextConverter(val fontManager: FontManager) {
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

            val outlineColor = attributedText.getOutlineColorAtIndex(it)
            if (outlineColor != null) {
                attributes.outlineColor = outlineColor
            }
            attributes.outlineWidth = attributedText.getOutlineWidthAtIndex(it)

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
    fun convert(attributedText: AttributedText,
                startingAttributes: FontAttributes,
                missingFontsTracker: MissingFontsTracker,
                disableTextReplacement: Boolean = false,
                density: Float = 1.0f): Spannable {
        val contentLengths = parseContentLengths(attributedText)
        val attributes = parseAttributes(attributedText, startingAttributes, contentLengths)
        val spannable = SpannableStringBuilder()

        for ((index, attribute) in attributes.withIndex()) {
            val content = attributedText.getContentAtIndex(index)
            val start = spannable.length
            spannable.append(content)
            val end = spannable.length

            val onTap = attributedText.getOnTapAtIndex(index)
            val onLayout = attributedText.getOnLayoutAtIndex(index)
            val imageAttachment = attributedText.getImageAttachmentAtIndex(index)

            if (imageAttachment != null && !disableTextReplacement) {
                spannable.setSpan(ImageAttachmentSpan(imageAttachment, density),
                        start,
                        end,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                spannable.append("\u2009") // thin space for line break opportunity
            } else {
                attribute.enumerateSpans(fontManager, missingFontsTracker, disableTextReplacement) {
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
        for (i in 0 until partsSize) {
            val attributes = fontAttributesArray[i]
            val chunkLength = contentLengths[i]
            var remainingChunkLength = chunkLength

            while (remainingChunkLength > 0) {
                val lineIndex = layout.getLineForOffset(currentOffset)
                val lineStart = layout.getLineStart(lineIndex)
                val lineEnd = layout.getLineEnd(lineIndex)

                // used to calculate the current text that's part of the current chunk, on the current line
                val chunkStartInLine = Math.max(currentOffset, lineStart)
                val chunkEndInLine = Math.min(currentOffset + remainingChunkLength, lineEnd)
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
}
