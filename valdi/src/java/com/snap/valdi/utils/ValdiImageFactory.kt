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
    fun fromResources(resources: Resources, resourceId: Int): ValdiImage {
        return createImage(BitmapFactory.decodeResource(resources, resourceId))
    }

    @JvmStatic
    fun fromByteArray(byteArray: ByteArray): ValdiImage {
        return createImage(BitmapFactory.decodeByteArray(byteArray, 0, byteArray.size))
    }

    @JvmStatic
    fun fromBitmap(bitmap: Bitmap): ValdiImage {
        return createImage(bitmap)
    }

    @JvmStatic
    fun fromFilePath(filePath: String): ValdiImage {
        return createImage(BitmapFactory.decodeFile(filePath))
    }
}
