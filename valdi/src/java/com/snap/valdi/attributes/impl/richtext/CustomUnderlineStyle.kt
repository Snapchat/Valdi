package com.snap.valdi.attributes.impl.richtext

import com.snap.valdi.exceptions.AttributeError

data class CustomUnderlineStyle(
    val height: Float,
    val onWidth: Float,
    val offWidth: Float,
    val offset: Float
) {
    val isPatterned: Boolean
        get() = onWidth > 0f && offWidth > 0f

    companion object {
        fun parse(value: String): CustomUnderlineStyle {
            val values = value.split(" ")
            if (values.size != 4) {
                throw AttributeError("customUnderlineStyle must contain exactly four numbers: height onWidth offWidth offset")
            }

            val height = parseNumber(values[0])
            val onWidth = parseNumber(values[1])
            val offWidth = parseNumber(values[2])
            val offset = parseNumber(values[3])

            if (height <= 0f) {
                throw AttributeError("customUnderlineStyle height must be positive")
            }

            val solid = onWidth == 0f && offWidth == 0f
            val patterned = onWidth > 0f && offWidth > 0f
            if (!solid && !patterned) {
                throw AttributeError("customUnderlineStyle onWidth and offWidth must both be positive, or both be 0")
            }

            return CustomUnderlineStyle(height, onWidth, offWidth, offset)
        }

        private fun parseNumber(value: String): Float {
            val number = value.toFloatOrNull()
                ?: throw AttributeError("Invalid customUnderlineStyle number '$value'")
            if (!number.isFinite()) {
                throw AttributeError("customUnderlineStyle values must be finite numbers")
            }
            return number
        }
    }
}
