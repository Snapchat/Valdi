package com.snap.valdi.bundle

import android.content.Context
import android.net.Uri
import androidx.annotation.Keep
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.BridgeCallUtils
import com.snap.valdi.utils.ValdiImage
import com.snap.valdi.utils.ValdiImageLoadOptions
import com.snap.valdi.utils.ValdiAssetLoadOutputType
import com.snap.valdi.utils.ValdiAssetLoader
import com.snap.valdi.utils.ValdiImageLoader
import com.snap.valdi.utils.ValdiVideoLoader
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.Disposable
import com.snap.valdi.utils.info
import com.snapchat.client.valdi_core.Cancelable

import android.util.Log
import android.graphics.Bitmap
import com.snap.valdi.imageloading.ValdiImageLoaderPostprocessor
import com.snap.valdi.utils.ValdiImageFactory

/**
 * Loads resources on behalf of Valdi C++. It is currently used only to resolve images.
 * Methods in this class are used by native code. Don't remove them!
 */
class ResourceResolver(private val context: Context,
                       private val coordinateResolver: CoordinateResolver,
                       private val postprocessor: ValdiImageLoaderPostprocessor,
                       private val logger: Logger,
                       private val customModuleProvider: IValdiCustomModuleProvider?) {

    @Keep
    fun getCustomModuleData(path: String, marshallerHandle: Long): ByteArray? {
        if (customModuleProvider == null) {
            return null
        }

        return BridgeCallUtils.handleCall(marshallerHandle) {
            customModuleProvider.getCustomModuleData(path)
        }
    }

    @Keep
    fun resolveResource(moduleName: String, resourcePath: String): Any? {
        val currentTime = System.nanoTime()
        val resourceId = ValdiImage.getImageIdentifier(context, moduleName, resourcePath)
        if (resourceId == 0) {
            return null
        }

        val imageIdentifierTime = System.nanoTime()

        val totalTimeMs = ((imageIdentifierTime - currentTime) / 1000).toDouble() / 1000.0

        logger.info("Loaded image $moduleName/$resourcePath (took ${totalTimeMs}ms)")
        return LocalAssetLoader.valdiAssetUrlFromResID(resourceId).toString()
    }

    @Keep
    fun requestPayloadFromURL(imageLoader: Any, url: String, marshallerHandle: Long): Any? {
        return BridgeCallUtils.handleCall(marshallerHandle) {
            (imageLoader as ValdiAssetLoader).getRequestPayload(Uri.parse(url))
        }
    }

    /**
    Called by C++ to perform a load from an image loader
     */
    @Keep
    fun loadAsset(imageLoader: Any,
                  requestPayload: Any,
                  preferredWidth: Int,
                  preferredHeight: Int,
                  colorMatrixFilter: Any?,
                  blurRadiusFilter: Float,
                  outputType: Int,
                  completionHandle: Long): Any? {

        val loadOutputType = when(outputType) {
            ValdiAssetLoadOutputType.BITMAP.value -> ValdiAssetLoadOutputType.BITMAP
            ValdiAssetLoadOutputType.RAW_CONTENT.value -> ValdiAssetLoadOutputType.RAW_CONTENT
            ValdiAssetLoadOutputType.VIDEO.value -> ValdiAssetLoadOutputType.VIDEO
            else -> ValdiAssetLoadOutputType.VIDEO
        }

        var disposable: Disposable? = null
        if (loadOutputType == ValdiAssetLoadOutputType.VIDEO) {
            val completion = AssetVideoLoaderCompletion(completionHandle)
            disposable = (imageLoader as ValdiVideoLoader).loadVideo(requestPayload, completion) ?: return null
        } else {
            val completion = AssetImageLoaderCompletion(completionHandle, colorMatrixFilter as? FloatArray)
            val options = ValdiImageLoadOptions(
                preferredWidth,
                preferredHeight,
                coordinateResolver.toPixelF(blurRadiusFilter),
                loadOutputType
            )
            disposable =
                (imageLoader as ValdiImageLoader).loadImage(requestPayload, options, completion)
                    ?: return null
        }

        if (disposable is Cancelable) {
            return disposable
        }

        return object: Cancelable() {
            override fun cancel() {
                disposable.dispose()
            }
        }
    }

    /**
    Called by C++ to perform a load from a byte array into a ValdiImage
     */
    @Keep
    fun loadAssetFromBytes(bytes: ByteArray,
                           preferredWidth: Int,
                           preferredHeight: Int,
                           colorMatrixFilter: Any?,
                           blurRadiusFilter: Float,
                           completionHandle: Long): Any? {
        return loadBitmapAsset(preferredWidth, preferredHeight, colorMatrixFilter, blurRadiusFilter, completionHandle) {
            ValdiImageFactory.fromByteArray(bytes)
        }
    }

    /**
    Called by C++ to perform a load from an already-rasterized bitmap into a ValdiImage.
     */
    @Keep
    fun loadAssetFromBitmap(bitmap: Any,
                            preferredWidth: Int,
                            preferredHeight: Int,
                            colorMatrixFilter: Any?,
                            blurRadiusFilter: Float,
                            completionHandle: Long): Any? {
        return loadBitmapAsset(preferredWidth, preferredHeight, colorMatrixFilter, blurRadiusFilter, completionHandle) {
            ValdiImageFactory.fromBitmap(bitmap as Bitmap)
        }
    }

    private fun loadBitmapAsset(preferredWidth: Int,
                                preferredHeight: Int,
                                colorMatrixFilter: Any?,
                                blurRadiusFilter: Float,
                                completionHandle: Long,
                                createImage: () -> ValdiImage): Any? {
        val completion = AssetImageLoaderCompletion(completionHandle, colorMatrixFilter as? FloatArray)
        val options = ValdiImageLoadOptions(
            preferredWidth,
            preferredHeight,
            coordinateResolver.toPixelF(blurRadiusFilter),
            ValdiAssetLoadOutputType.BITMAP
        )

        this.postprocessor.executor.submit {
            val valdiImage = try {
                createImage()
            } catch (err: Throwable) {
                completion.onImageLoadComplete(0, 0, null, err)
                return@submit
            }

            this.postprocessor.postprocess(valdiImage, options, completion)
        }

        return null
    }

}
