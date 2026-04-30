package com.snap.valdi.attributes.impl.richtext

/**
 * AttributedText is the Java counter part of
 * Valdi::TextAttributeValue. It contains a list of parts
 * where each part has a string content and an associated style.
 */
import com.snap.valdi.callable.ValdiFunction

interface AttributedText {
    /**
     * Returns the number of parts within the AttributedText
     */
    fun getPartsSize(): Int

    /**
     * Return the string content for the part at the given index
     */
    fun getContentAtIndex(index: Int): String

    /**
     * Return the font name for the part at the given index, or null if unspecified.
     */
    fun getFontAtIndex(index: Int): String?

    /**
     * Return the text decoration for the part at the given index, or null if unspecified
     */
    fun getTextDecorationAtIndex(index: Int): TextDecoration?

    /**
     * Return the text color for the part at the given index, or null if unspecified.
     */
    fun getColorAtIndex(index: Int): Int?
    fun getOnTapAtIndex(index: Int): ValdiFunction?
    fun getOnLayoutAtIndex(index: Int): ValdiFunction?


    /**
     * Return the text outline color for the part at the given index, or null if unspecified.
     */
    fun getOutlineColorAtIndex(index: Int): Int?
    /**
     * Return the text outline width for the part at the given index, or 0F if unspecified.
     */
    fun getOutlineWidthAtIndex(index: Int): Float

    /*
     * For attributed text with outline + fill, in edit text scenarios, we choose to draw an outline
     * This is to preserve selection/calcs of the initial text
     */
    fun hasOutline(): Boolean

    /**
     * Return the image attachment info for the part at the given index, or null if not an image attachment.
     */
    fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo?
}
