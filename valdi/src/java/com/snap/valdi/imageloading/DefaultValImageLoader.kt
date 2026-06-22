package com.snap.valdi.imageloading

import android.content.Context
import android.net.Uri
import android.util.Base64
import com.snap.valdi.bundle.LocalAssetLoader
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.utils.ValdiAssetLoadOutputType
import com.snap.valdi.utils.ValdiImage
import com.snap.valdi.utils.ValdiImageContent
import com.snap.valdi.utils.ValdiImageFactory
import com.snap.valdi.utils.ValdiImageLoadCompletion
import com.snap.valdi.utils.ValdiImageLoadOptions
import com.snap.valdi.utils.ValdiImageLoader
import com.snap.valdi.utils.ValdiSVGRasterizer
import com.snap.valdi.utils.ValdiImageWithContent
import com.snap.valdi.utils.DelegatedLoader
import com.snap.valdi.utils.Disposable
import com.snap.valdi.utils.LoadCompletion
import com.snap.valdi.utils.LoaderDelegate
import com.snapchat.client.valdi_core.HTTPRequest
import com.snapchat.client.valdi_core.HTTPRequestManagerCompletion
import com.snapchat.client.valdi_core.HTTPResponse
import com.snapchat.client.valdi_core.HTTPRequestManager
import java.lang.ref.SoftReference

/**
 * An inefficient default image loader, not designed to use in production.
 */
class DefaultValdiImageLoader(val context: Context,
                                 val postprocessor: ValdiImageLoaderPostprocessor,
                                 val requestManager: HTTPRequestManager,
                                 private val svgRasterizer: ValdiSVGRasterizer): ValdiImageLoader {

    private data class CacheKey(val uri: Uri,
                                val outputType: ValdiAssetLoadOutputType
    )

    private data class LoadRequest(val cacheKey: CacheKey,
                                   val requestedWidth: Int,
                                   val requestedHeight: Int
    )

    private val cache = hashMapOf<CacheKey, SoftReference<ValdiImage>>()

    private val loaderImpl = object: LoaderDelegate<LoadRequest, ValdiImage> {
        override fun load(request: LoadRequest, completion: LoadCompletion<ValdiImage>) {
            val cacheKey = request.cacheKey
            val cachedImage = getFromCache(cacheKey)
            if (cachedImage != null) {
                completion.onSuccess(cachedImage)
                return
            }

            val uri = cacheKey.uri

            if (LocalAssetLoader.isValdiAssetUrl(uri)) {
                val resId = LocalAssetLoader.resIDFromValdiAssetUrl(uri)
                loadImageResource(completion, cacheKey.outputType, resId)
            } else if (uri.scheme == "file") {
                loadImageFromFilePath(completion, cacheKey.outputType, uri.path ?: "")
            } else if (uri.scheme == "data") {
                loadImageFromDataScheme(completion, cacheKey.outputType, uri, request.requestedWidth, request.requestedHeight)
            } else {
                loadImageUri(completion, cacheKey.outputType, uri, request.requestedWidth, request.requestedHeight)
            }
        }
    }

    private val loader = DelegatedLoader(loaderImpl, postprocessor.executor)

    private fun createValdiImage(completion: LoadCompletion<ValdiImage>, createFunc: () -> ValdiImage) {
        try {
            val valdiImage = createFunc()
            completion.onSuccess(valdiImage)
        } catch (exception: Exception) {
            completion.onFailure(exception)
        }
    }

    private fun loadImageResource(completion: LoadCompletion<ValdiImage>, outputType: ValdiAssetLoadOutputType, resourceId: Int) {
        when (outputType) {
            ValdiAssetLoadOutputType.BITMAP -> {
                createValdiImage(completion) {
                    ValdiImageFactory.fromResources(context.resources, resourceId, svgRasterizer)
                }
            }
            ValdiAssetLoadOutputType.RAW_CONTENT -> {
                try {
                    val content = ValdiImageContent.fromStream(context.resources.openRawResource(resourceId))
                    completion.onSuccess(ValdiImageWithContent(content))
                } catch (exception: Exception) {
                    completion.onFailure(exception)
                }
            }
            else -> {
                // Something has gone horribly wrong
            }
        }
    }

    private fun loadImageFromData(completion: LoadCompletion<ValdiImage>, outputType: ValdiAssetLoadOutputType, body: ByteArray?, requestedWidth: Int, requestedHeight: Int) {
        if (body == null) {
            completion.onFailure(ValdiException("Did not receive response body"))
            return
        }

        when (outputType) {
            ValdiAssetLoadOutputType.BITMAP -> {
                createValdiImage(completion) {
                    ValdiImageFactory.fromByteArray(body, requestedWidth, requestedHeight)
                }
            }

            ValdiAssetLoadOutputType.RAW_CONTENT -> {
                completion.onSuccess(ValdiImageWithContent(ValdiImageContent.Bytes(body)))
            }
            else -> {
                // Something has gone horribly wrong
            }
        }
    }

    private fun loadImageFromFilePath(completion: LoadCompletion<ValdiImage>, outputType: ValdiAssetLoadOutputType, filePath: String) {
        when (outputType) {
            ValdiAssetLoadOutputType.BITMAP -> {
                createValdiImage(completion) {
                    ValdiImageFactory.fromFilePath(filePath, context.resources.displayMetrics.density)
                }
            }

            ValdiAssetLoadOutputType.RAW_CONTENT -> {
                completion.onSuccess(ValdiImageWithContent(ValdiImageContent.FileReference(filePath)))
            }
            else -> {
                // Something has gone horribly wrong
            }
        }
    }

    private fun loadImageFromDataScheme(completion: LoadCompletion<ValdiImage>, outputType: ValdiAssetLoadOutputType, uri: Uri, requestedWidth: Int, requestedHeight: Int) {
        val str = uri.toString()
        val delimiter = "base64,"
        val index = str.indexOf(delimiter)
        if (index < 0) {
            completion.onFailure(ValdiException("Invalid data URL, expecting base64"))
            return
        }
        
        val bytes = try {
            Base64.decode(str.substring(index + delimiter.length), Base64.DEFAULT)
        } catch (err: Throwable) {
            completion.onFailure(err)
            return
        }

        loadImageFromData(completion, outputType, bytes, requestedWidth, requestedHeight)
    }

    private fun loadImageUri(completion: LoadCompletion<ValdiImage>, outputType: ValdiAssetLoadOutputType, url: Uri, requestedWidth: Int, requestedHeight: Int) {
        requestManager.performRequest(HTTPRequest(url.toString(), "GET", null, null, 0), object: HTTPRequestManagerCompletion() {

            override fun onComplete(response: HTTPResponse) {
                loadImageFromData(completion, outputType, response.body, requestedWidth, requestedHeight)
            }

            override fun onFail(error: String) {
                completion.onFailure(ValdiException(error))
            }
        })
    }

    private fun getFromCache(url: CacheKey): ValdiImage? {
        return synchronized(cache) {
            cache[url]?.get()
        }
    }

    private fun storeInCache(url: CacheKey, image: ValdiImage) {
        val previousValue = synchronized(cache) {
            val previousValue = cache[url]?.get()
            cache[url] = SoftReference(image)

            previousValue
        }

        image.retain()
        previousValue?.release()
    }

    override fun getSupportedURLSchemes(): List<String> {
        return listOf("file", "http", "https", "data", LocalAssetLoader.VALDI_ASSET_SCHEME)
    }

    override fun getRequestPayload(url: Uri): Any {
        return url
    }

    private fun onImageLoaded(image: ValdiImage, options: ValdiImageLoadOptions, completion: ValdiImageLoadCompletion) {
        this.postprocessor.postprocess(image, options, completion)
    }

    override fun getSupportedOutputTypes(): Int {
        return ValdiAssetLoadOutputType.BITMAP.value or ValdiAssetLoadOutputType.RAW_CONTENT.value
    }

    override fun loadImage(requestPayload: Any, options: ValdiImageLoadOptions, completion: ValdiImageLoadCompletion): Disposable? {
        val url = requestPayload as Uri
        val requestedWidth = if (options.outputType == ValdiAssetLoadOutputType.BITMAP) options.requestedWidth else 0
        val requestedHeight = if (options.outputType == ValdiAssetLoadOutputType.BITMAP) options.requestedHeight else 0
        val cacheKey = CacheKey(url, options.outputType)
        val image = getFromCache(cacheKey)
        if (image != null) {
            onImageLoaded(image, options, completion)
            return null
        }

        val loadRequest = LoadRequest(cacheKey, requestedWidth, requestedHeight)
        return loader.load(loadRequest, object: LoadCompletion<ValdiImage> {

            override fun onSuccess(item: ValdiImage) {
                storeInCache(cacheKey, item)

                if (options.outputType == ValdiAssetLoadOutputType.BITMAP) {
                    onImageLoaded(item, options, completion)
                } else {
                    completion.onImageLoadComplete(0, 0, item, null)
                }
            }

            override fun onFailure(error: Throwable) {
                completion.onImageLoadComplete(0, 0, null, error)
            }
        })
    }
}
