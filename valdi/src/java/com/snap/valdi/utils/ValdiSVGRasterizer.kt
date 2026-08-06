package com.snap.valdi.utils

import android.content.res.Resources
import android.util.LruCache
import com.snap.valdi.exceptions.ValdiException

class ValdiSVGRasterizer(maxCacheSizeInBytes: Int) {

    private class RetainingCache(maxSizeBytes: Int) : LruCache<String, ValdiImage>(maxSizeBytes) {
        override fun sizeOf(key: String, value: ValdiImage): Int {
            val bitmap = value.getContentAsBitmap()
            val byteCount = bitmap?.byteCount ?: 1
            return if (byteCount > 0) byteCount else 1
        }

        // Balances the retain() taken in rasterizeLocalSVG. Without it the cache's reference is
        // never given back and the bitmap outlives the cache entry.
        override fun entryRemoved(evicted: Boolean, key: String, oldValue: ValdiImage, newValue: ValdiImage?) {
            oldValue.release()
        }
    }

    private val cache: RetainingCache by lazy {
        RetainingCache(maxCacheSizeInBytes)
    }

    fun rasterizeLocalResource(resources: Resources, resourceId: Int): ValdiImage {
        val cacheKey = "${resources.getResourcePackageName(resourceId)}:$resourceId"
        return rasterizeLocalSVG(cacheKey, resources.displayMetrics.density) {
            try {
                resources.openRawResource(resourceId).use { it.readBytes() }
            } catch (exception: Exception) {
                throw ValdiException("Failed to read local SVG resource $cacheKey", exception)
            }
        }
    }

    private fun rasterizeLocalSVG(cacheKey: String, displayScale: Float, loadData: () -> ByteArray): ValdiImage {
        cache.get(cacheKey)?.let { return it }

        synchronized(cache) {
            cache.get(cacheKey)?.let { return it }
            val byteArray = loadData()
            if (!ValdiSVGRenderer.isSVG(byteArray)) {
                throw ValdiException("Local SVG asset $cacheKey does not contain SVG data")
            }

            val image = ValdiImageWithBitmap(ValdiSVGRenderer.rasterizeSVG(byteArray, 0, 0, displayScale))
            cache.put(cacheKey, image)
            // The cache needs its own reference: callers balance retain()/release() around the image
            // they get back (see DefaultValImageLoader and AssetImageLoaderCompletion), so without
            // this the count returns to 0, ValdiImage.destroy() recycles the bitmap, and the next
            // cache hit hands out a recycled one.
            //
            // Deliberately after put(): a zero-size cache evicts during put(), and retaining first
            // would let that eviction's release() drop the count to 0 and destroy the image we are
            // about to return.
            image.retain()
            return image
        }
    }
}
