package com.snap.valdi.attributes.impl.richtext

import androidx.annotation.Keep
import com.snap.valdi.attributes.conversions.ColorConversions
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.exceptions.ValdiFatalException
import com.snapchat.client.valdi.utils.CppObjectWrapper

@Keep
class AttributedTextCpp(private val native: CppObjectWrapper): AttributedText {
    override fun getPartsSize(): Int {
        return nativeGetPartsSize(native.nativeHandle)
    }

    override fun getContentAtIndex(index: Int): String {
        return nativeGetContent(native.nativeHandle, index)
    }

    override fun getFontAtIndex(index: Int): String? {
        return nativeGetFont(native.nativeHandle, index)
    }

    override fun getTextDecorationAtIndex(index: Int): TextDecoration? {
        val textDecorationInt = nativeGetTextDecoration(native.nativeHandle, index)
        return when (textDecorationInt) {
            TEXT_DECORATION_UNSET -> null
            TEXT_DECORATION_NONE -> TextDecoration.NONE
            TEXT_DECORATION_UNDERLINE -> TextDecoration.UNDERLINE
            TEXT_DECORATION_STRIKETHROUGH -> TextDecoration.STRIKETHROUGH
            TEXT_DECORATION_DASHED_UNDERLINE -> TextDecoration.DASHED_UNDERLINE
            TEXT_DECORATION_DOTTED_UNDERLINE -> TextDecoration.DOTTED_UNDERLINE
            else -> ValdiFatalException.handleFatal("Invalid textDecoration $textDecorationInt")
        }
    }

    override fun getColorAtIndex(index: Int): Int? {
        val colorLong = nativeGetColor(native.nativeHandle, index)
        if (colorLong == Long.MIN_VALUE) {
            return null
        }

        return ColorConversions.fromRGBA(colorLong)
    }

    override fun getBackgroundColorAtIndex(index: Int): Int? {
        val colorLong = nativeGetBackgroundColor(native.nativeHandle, index)
        if (colorLong == Long.MIN_VALUE) {
            return null
        }

        return ColorConversions.fromRGBA(colorLong)
    }

    override fun getOnTapAtIndex(index: Int): ValdiFunction? {
        return nativeGetOnTap(native.nativeHandle, index) as? ValdiFunction
    }

    override fun getOnLayoutAtIndex(index: Int): ValdiFunction? {
        return nativeGetOnLayout(native.nativeHandle, index) as? ValdiFunction
    }

    override fun getOutlineColorAtIndex(index: Int): Int? {
        val colorLong = nativeGetOutlineColor(native.nativeHandle, index)
        if (colorLong == Long.MIN_VALUE) {
            return null
        }

        return ColorConversions.fromRGBA(colorLong)
    }

    override fun getOutlineWidthAtIndex(index: Int): Float {
        return nativeGetOutlineWidth(native.nativeHandle, index).toFloat()
    }

    override fun hasOutline(): Boolean {
        val partsSize = getPartsSize()

        for (index in 0 until partsSize) {
            val outlineColor = getOutlineColorAtIndex(index)
            val outlineWidth = getOutlineWidthAtIndex(index)
            if (outlineColor != null && outlineWidth > 0) {
                return true
            }
        }

        return false
    }

    override fun getAnimationTransformsSize(): Int {
        return nativeGetAnimationTransformsSize(native.nativeHandle)
    }

    override fun getImageAttachmentAtIndex(index: Int): ImageAttachmentInfo? {
        val width = nativeGetImageAttachmentWidth(native.nativeHandle, index)
        if (width <= 0) {
            return null
        }
        val height = nativeGetImageAttachmentHeight(native.nativeHandle, index)
        val imageData = nativeGetImageAttachmentData(native.nativeHandle, index)
        return ImageAttachmentInfo(width, height, imageData)
    }

    override fun getInlineViewAttachmentAtIndex(index: Int): InlineViewAttachmentInfo? {
        val childIndex = nativeGetInlineViewAttachmentChildIndex(native.nativeHandle, index)
        if (childIndex < 0) {
            return null
        }
        val verticalAlignment = InlineViewVerticalAlignment.fromRawValue(
            nativeGetInlineViewAttachmentVerticalAlignment(native.nativeHandle, index)
        )
        val width = nativeGetInlineViewAttachmentWidth(native.nativeHandle, index)
        val height = nativeGetInlineViewAttachmentHeight(native.nativeHandle, index)
        return InlineViewAttachmentInfo(childIndex, verticalAlignment, width, height)
    }

    override fun getAnimationTransformAtIndex(index: Int): TextAnimationTransform? {
        if (!nativeHasAnimationTransform(native.nativeHandle, index)) {
            return null
        }

        return TextAnimationTransform(
            key = nativeGetAnimationTransformKey(native.nativeHandle, index),
            translationY = nativeGetAnimationTransformTranslationY(native.nativeHandle, index),
            scale = nativeGetAnimationTransformScale(native.nativeHandle, index),
            opacity = nativeGetAnimationTransformOpacity(native.nativeHandle, index),
            duration = nativeGetAnimationTransformDuration(native.nativeHandle, index),
            timeOffsetBetweenParts = nativeGetAnimationTransformTimeOffsetBetweenParts(native.nativeHandle, index),
            groupIndex = nativeGetAnimationTransformGroupIndex(native.nativeHandle, index),
            partIndexInGroup = nativeGetAnimationTransformPartIndexInGroup(native.nativeHandle, index),
            partPattern = nativeGetAnimationTransformPartPattern(native.nativeHandle, index)
        )
    }

    companion object {
        private const val TEXT_DECORATION_UNSET = Int.MIN_VALUE
        private const val TEXT_DECORATION_NONE = 0
        private const val TEXT_DECORATION_UNDERLINE = 1
        private const val TEXT_DECORATION_STRIKETHROUGH = 2
        private const val TEXT_DECORATION_DASHED_UNDERLINE = 3
        private const val TEXT_DECORATION_DOTTED_UNDERLINE = 4
        @JvmStatic
        private external fun nativeGetPartsSize(nativeHandle: Long): Int
        @JvmStatic
        private external fun nativeGetContent(nativeHandle: Long, index: Int): String
        @JvmStatic
        private external fun nativeGetFont(nativeHandle: Long, index: Int): String?
        @JvmStatic
        private external fun nativeGetTextDecoration(nativeHandle: Long, index: Int): Int
        @JvmStatic
        private external fun nativeGetColor(nativeHandle: Long, index: Int): Long
        @JvmStatic
        private external fun nativeGetBackgroundColor(nativeHandle: Long, index: Int): Long
        @JvmStatic
        private external fun nativeGetOutlineColor(nativeHandle: Long, index: Int): Long
        @JvmStatic
        private external fun nativeGetOutlineWidth(nativeHandle: Long, index: Int): Double
        @JvmStatic
        private external fun nativeGetOnTap(nativeHandle: Long, index: Int): Any?
        @JvmStatic
        private external fun nativeGetOnLayout(nativeHandle: Long, index: Int): Any?
        @JvmStatic
        private external fun nativeGetImageAttachmentWidth(nativeHandle: Long, index: Int): Float
        @JvmStatic
        private external fun nativeGetImageAttachmentHeight(nativeHandle: Long, index: Int): Float
        @JvmStatic
        private external fun nativeGetImageAttachmentData(nativeHandle: Long, index: Int): ByteArray?
        @JvmStatic
        private external fun nativeGetInlineViewAttachmentChildIndex(nativeHandle: Long, index: Int): Int
        @JvmStatic
        private external fun nativeGetInlineViewAttachmentVerticalAlignment(nativeHandle: Long, index: Int): Int
        @JvmStatic
        private external fun nativeGetInlineViewAttachmentWidth(nativeHandle: Long, index: Int): Float
        @JvmStatic
        private external fun nativeGetInlineViewAttachmentHeight(nativeHandle: Long, index: Int): Float
        @JvmStatic
        private external fun nativeGetAnimationTransformsSize(nativeHandle: Long): Int
        @JvmStatic
        private external fun nativeHasAnimationTransform(nativeHandle: Long, index: Int): Boolean
        @JvmStatic
        private external fun nativeGetAnimationTransformKey(nativeHandle: Long, index: Int): String?
        @JvmStatic
        private external fun nativeGetAnimationTransformTranslationY(nativeHandle: Long, index: Int): Float
        @JvmStatic
        private external fun nativeGetAnimationTransformScale(nativeHandle: Long, index: Int): Float
        @JvmStatic
        private external fun nativeGetAnimationTransformOpacity(nativeHandle: Long, index: Int): Float
        @JvmStatic
        private external fun nativeGetAnimationTransformDuration(nativeHandle: Long, index: Int): Double
        @JvmStatic
        private external fun nativeGetAnimationTransformTimeOffsetBetweenParts(nativeHandle: Long, index: Int): Double
        @JvmStatic
        private external fun nativeGetAnimationTransformGroupIndex(nativeHandle: Long, index: Int): Int
        @JvmStatic
        private external fun nativeGetAnimationTransformPartIndexInGroup(nativeHandle: Long, index: Int): Int
        @JvmStatic
        private external fun nativeGetAnimationTransformPartPattern(nativeHandle: Long, index: Int): String?
    }
}
