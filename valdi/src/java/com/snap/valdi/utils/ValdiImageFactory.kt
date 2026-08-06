package com.snap.valdi.utils

import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import com.snap.valdi.exceptions.ValdiException

object ValdiImageFactory {

    @JvmStatic
    private fun createImage(bitmap: Bitmap?): ValdiImage {
        if (bitmap == null) {
            throw ValdiException("Failed to decode image")
        }
        return ValdiImageWithBitmap(bitmap)
    }

    @JvmStatic
    fun fromResources(resources: Resources, resourceId: Int, svgRasterizer: ValdiSVGRasterizer): ValdiImage {
        val resourceType = resources.getResourceTypeName(resourceId)
        if (resourceType == "raw") {
            return svgRasterizer.rasterizeLocalResource(resources, resourceId)
        }

        return createImage(BitmapFactory.decodeResource(resources, resourceId))
    }

    @JvmStatic
    fun fromByteArray(byteArray: ByteArray): ValdiImage {
        return fromByteArray(byteArray, 0, 0)
    }

    @JvmStatic
    fun fromByteArray(byteArray: ByteArray, preferredWidth: Int, preferredHeight: Int): ValdiImage {
        if (ValdiSVGRenderer.isSVG(byteArray)) {
            return ValdiImageWithBitmap(ValdiSVGRenderer.rasterizeSVG(byteArray, preferredWidth, preferredHeight))
        }

        return createImage(BitmapFactory.decodeByteArray(byteArray, 0, byteArray.size))
    }

    @JvmStatic
    fun fromBitmap(bitmap: Bitmap): ValdiImage {
        return createImage(bitmap)
    }

    @JvmStatic
    fun fromFilePath(filePath: String, displayScale: Float): ValdiImage {
        if (filePath.endsWith(".svg", ignoreCase = true)) {
            return createImage(ValdiSVGRenderer.rasterizeSVGFromFilePath(filePath, displayScale))
        }

        return createImage(BitmapFactory.decodeFile(filePath))
    }
}
