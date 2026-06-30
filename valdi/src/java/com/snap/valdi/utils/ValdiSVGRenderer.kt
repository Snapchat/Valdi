package com.snap.valdi.utils

import android.graphics.Bitmap
import com.snap.valdi.exceptions.ValdiException
import com.snapchat.client.valdi.NativeBridge

object ValdiSVGRenderer {
    private val SVG_PREFIX = "<svg".encodeToByteArray()
    private val XML_PREFIX = "<?xml".encodeToByteArray()

    @JvmStatic
    fun isSVG(data: ByteArray): Boolean {
        var index = 0
        while (index < data.size && isWhitespace(data[index])) {
            index++
        }

        return data.hasPrefix(SVG_PREFIX, index) || data.hasPrefix(XML_PREFIX, index)
    }

    @JvmStatic
    fun rasterizeSVG(data: ByteArray, preferredWidth: Int, preferredHeight: Int): Bitmap {
        return rasterizeSVG(data, preferredWidth, preferredHeight, 1.0f)
    }

    @JvmStatic
    fun rasterizeSVG(data: ByteArray, preferredWidth: Int, preferredHeight: Int, displayScale: Float): Bitmap {
        return NativeBridge.rasterizeSVG(data, preferredWidth, preferredHeight, displayScale) as? Bitmap
            ?: throw ValdiException("Failed to rasterize SVG")
    }

    @JvmStatic
    fun rasterizeSVGFromFilePath(filePath: String, displayScale: Float): Bitmap {
        return NativeBridge.rasterizeSVGFromFilePath(filePath, displayScale) as? Bitmap
            ?: throw ValdiException("Failed to rasterize SVG file path=$filePath")
    }

    private fun isWhitespace(value: Byte): Boolean {
        return value == 32.toByte() ||
            value == 10.toByte()
    }

    private fun ByteArray.hasPrefix(prefix: ByteArray, startIndex: Int): Boolean {
        if (startIndex + prefix.size > size) {
            return false
        }

        for (index in prefix.indices) {
            if (this[startIndex + index] != prefix[index]) {
                return false
            }
        }

        return true
    }
}
