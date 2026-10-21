package com.snap.valdi.attributes.impl.fonts

import android.content.Context
import android.content.res.AssetFileDescriptor
import android.graphics.Typeface
import android.net.Uri
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.utils.*
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import kotlin.math.absoluteValue

class FontManager(val context: Context, private val typefaceResLoader: TypefaceResLoader) {

    interface Listener {
        /**
         * Called when a font is requested, before it is resolved
         */
        fun onFontRequested(descriptor: FontDescriptor)

        /**
         * Called when a typeface was loaded from a FontDescriptor
         */
        fun onTypefaceRegistered(descriptor: FontDescriptor,
                                 isFallback: Boolean,
                                 dataProvider: FontDataProvider,
                                 registerImmediatelyWithSnapDrawing: Boolean)
    }

    interface ModuleTypefaceDataProvider {
        fun getModuleTypefacePath(moduleName: String, path: String): String?
    }

    var listener: FontManager.Listener? = null

    private class RegisteredFont(val descriptor: FontDescriptor,
                                 var loader: FontLoader?,
                                 var typeface: Typeface?)

    private var registeredFontsByName = hashMapOf<String, RegisteredFont>()
    private var registeredFontsByFamily = hashMapOf<String, MutableList<RegisteredFont>>()
    private var moduleTypefaceDataProviders = mutableListOf<ModuleTypefaceDataProvider>()

    private val executor = ExecutorsUtil.newSingleThreadCachedExecutor { r ->
        Thread(r).apply {
            name = "Valdi Font Loader"
            priority = Thread.NORM_PRIORITY
        }
    }

    private data class FontLoadOperation(val fontDescriptor: FontDescriptor, val loader: FontLoader)

    private val loaderImpl = object: LoaderDelegate<FontLoadOperation, Typeface> {

        override fun load(key: FontLoadOperation, completion: LoadCompletion<Typeface>) {
            key.loader.load(object: LoadCompletion<Typeface> {
                override fun onSuccess(item: Typeface) {
                    doRegister(RegisteredFont(key.fontDescriptor, key.loader, item))

                    completion.onSuccess(item)
                }

                override fun onFailure(error: Throwable) {
                    completion.onFailure(error)
                }
            })
        }

    }

    private val loader = DelegatedLoader(loaderImpl, executor)

    private val mismatchStyleOffset = FontWeight.values().size

    private fun computeSelectionScore(desiredFont: FontDescriptor, candidate: FontDescriptor): Int {
        /**
         * This will return a score where values closer than 0 are higher matches.
         * In case of equality, values lower than 0 should be prioritized over values higher than 0.
         *
         * Examples:
         * Desired LIGHT
         * CANDIDATE: DEMI_BOLD
         * Score: -3
         * Score is negative because weight is lower than the desired weight
         *
         * Desired: MEDIUM
         * CANDIDATE: NORMAL
         * Score: 1
         * Score is positive because weight is higher than the desired weight

         * Desired: NORMAL
         * Candidate: NORMAL
         * Mismatched style
         * Score: 8
         * Mismatching styles look very bad, we prioritize matching styles over weights.
         *
         */

        var score = candidate.weight.ordinal - desiredFont.weight.ordinal
        if (desiredFont.style != candidate.style) {
            if (score >= 0) {
                score += mismatchStyleOffset
            } else {
                score -= mismatchStyleOffset
            }
        }
        return score
    }

    private fun resolveModuleFont(composedName: String): RegisteredFont? {
        val components = composedName.split(":")
        val moduleName = components[0]
        val fontName = components[1]
        for (moduleTypefaceDataProvider in moduleTypefaceDataProviders) {
            val fontPath = moduleTypefaceDataProvider.getModuleTypefacePath(moduleName, fontName)
            if (fontPath != null) {
                val fontLoader = object: FontLoader {
                    override fun load(completion: LoadCompletion<Typeface>) {
                        val typeface = try {
                            Typeface.createFromFile(fontPath)
                        } catch (exc: Throwable) {
                            completion.onFailure(exc)
                            return
                        }
                        completion.onSuccess(typeface)
                    }

                    override fun allowSynchronousLoad(): Boolean {
                        return true
                    }
                }
                return RegisteredFont(descriptor = FontDescriptor(name = composedName), loader = fontLoader, typeface = null)
            }
        }

        return null
    }

    private fun hasExactRegisteredFont(fontDescriptor: FontDescriptor): Boolean {
        synchronized(this) {
            if (fontDescriptor.name != null) {
                return registeredFontsByName.containsKey(fontDescriptor.name)
            }
            val family = fontDescriptor.family ?: "default"
            return registeredFontsByFamily[family]?.any { it.descriptor == fontDescriptor } == true
        }
    }

    private fun resolveFont(fontDescriptor: FontDescriptor): RegisteredFont? {
        if (fontDescriptor.name != null) {
            val font = registeredFontsByName[fontDescriptor.name]
            if (font != null) {
                return font
            }

            if (fontDescriptor.name.contains(":")) {
                val moduleFont = resolveModuleFont(fontDescriptor.name)
                if (moduleFont != null) {
                    registeredFontsByName[fontDescriptor.name] = moduleFont
                    return moduleFont
                }
            }

            return null
        }

        val family = fontDescriptor.family ?: "default"
        val availableFonts = registeredFontsByFamily[family] ?: return null

        var closestFontDescriptor: RegisteredFont? = null
        for (font in availableFonts) {
            val descriptor = font.descriptor

            if (closestFontDescriptor == null) {
                closestFontDescriptor = font
            } else {
                val previousScore = computeSelectionScore(fontDescriptor, closestFontDescriptor.descriptor)
                val newScore = computeSelectionScore(fontDescriptor, descriptor)

                val previousScoreAbs = previousScore.absoluteValue
                val newScoreAbs = newScore.absoluteValue

                // We are looking for the closest value to zero
                if (newScoreAbs < previousScoreAbs) {
                    closestFontDescriptor = font
                } else if (newScoreAbs == previousScoreAbs) {
                    // In case of equality, we choose the lower values, which means we prioritize
                    // weight which are less strong than our requested weight over the ones
                    // which are higher
                    if (newScore < previousScore) {
                        closestFontDescriptor = font
                    }
                }
            }
        }

        return closestFontDescriptor
    }

    /**
     * Load the font with the given resource id in the given context synchronously and register it.
     *
     */
    fun loadSyncAndRegister(fontDescriptor: FontDescriptor,
                            context: Context,
                            resId: Int,
                            isFallback: Boolean = false,
                            registerImmediatelyWithSnapDrawing: Boolean = false) {
        val fontLoader = object: FontLoader {
            override fun load(completion: LoadCompletion<Typeface>) {
                val typeface = try {
                    typefaceResLoader.loadTypeface(context, resId)
                } catch (exc: Throwable) {
                    completion.onFailure(exc)
                    return
                }
                completion.onSuccess(typeface)
            }

            override fun allowSynchronousLoad(): Boolean {
                return true
            }
        }

        val fontDataProvider = object: FontDataProvider {
            override fun openAssetFileDescriptor(): AssetFileDescriptor? {
                return null
            }

            override fun getBytes(): ByteArray? {
                return context.resources.openRawResource(resId).use {
                    it.readBytes()
                }
            }
        }

        registerWithDataProvider(
            fontDescriptor,
            fontDataProvider,
            fontLoader,
            isFallback,
            registerImmediatelyWithSnapDrawing,
        )
    }

    /**
     * Load the font with the given URL synchronously and register it.
     */
    fun loadSyncAndRegister(fontDescriptor: FontDescriptor,
                            uri: Uri,
                            isFallback: Boolean) {
        val fontDataProvider = object: FontDataProvider {
            override fun openAssetFileDescriptor(): AssetFileDescriptor? {
                return context.contentResolver.openAssetFileDescriptor(uri, "r")
                        ?: throw ValdiException("Could not open assetFileDescriptor with font ${fontDescriptor} with URI ${uri}")
            }

            override fun getBytes(): ByteArray? {
                return null
            }
        }

        registerWithDataProvider(fontDescriptor, fontDataProvider, null, isFallback)
    }

    /**
     * Registers a Font with its font bytes with its associated Android Typeface object.
     * If the Typeface object is not passed in, the Font will only be available within SnapDrawing.
     */
    fun registerWithDataProvider(fontDescriptor: FontDescriptor,
                                 fontDataProvider: FontDataProvider?,
                                 fontLoader: FontLoader?,
                                 isFallback: Boolean,
                                 registerImmediatelyWithSnapDrawing: Boolean = false) {
        if (fontDataProvider != null) {
            listener?.onTypefaceRegistered(
                fontDescriptor,
                isFallback,
                fontDataProvider,
                registerImmediatelyWithSnapDrawing,
            )
        }

        if (fontLoader != null) {
            register(fontDescriptor, fontLoader)
        }
    }

    /**
     * Register an already loaded Typeface given a FontDescriptor, which makes the Typeface
     * immediately available to Valdi components which wants to use it.
     */
    fun register(fontDescriptor: FontDescriptor, typeface: Typeface) {
        doRegister(RegisteredFont(fontDescriptor, null, typeface))
    }

    /**
     * Register a FontLoader given a FontDescriptor. The loader will be called when the font
     * is required for the first time.
     */
    fun register(fontDescriptor: FontDescriptor, loader: FontLoader) {
        doRegister(RegisteredFont(fontDescriptor, loader, null))
    }

    private fun doRegister(registeredFont: RegisteredFont) {
        val fontDescriptor = registeredFont.descriptor

        synchronized(this) {
            if (fontDescriptor.name != null) {
                registeredFontsByName[fontDescriptor.name] = registeredFont
            }
            if (fontDescriptor.family != null) {
                var otherFonts = registeredFontsByFamily[fontDescriptor.family]
                if (otherFonts == null) {
                    otherFonts = mutableListOf()
                    registeredFontsByFamily[fontDescriptor.family] = otherFonts
                    otherFonts.add(registeredFont)
                } else {
                    val conflictingFont = otherFonts.firstOrNull { it.descriptor == fontDescriptor }
                    if (conflictingFont == null) {
                        otherFonts.add(registeredFont)
                    } else {
                        if (registeredFont.loader != null) {
                            conflictingFont.loader = registeredFont.loader
                        }
                        if (registeredFont.typeface != null) {
                            conflictingFont.typeface = registeredFont.typeface
                        }
                    }
                }
            }
        }
    }

    /**
     * Get an already loaded Typeface given the FontDescriptor.
     */
    fun get(fontDescriptor: FontDescriptor): Typeface? {
        // onFontRequested blocks the caller until the whole Valdi worker queue drains, which takes
        // seconds on low-end devices under load. Only pay for it when a pending load operation could
        // still register this font.
        if (!hasExactRegisteredFont(fontDescriptor)) {
            listener?.onFontRequested(fontDescriptor)
        }

        val typeface: Typeface?
        val fontLoader: FontLoader?
        synchronized(this) {
            val registeredFont = resolveFont(fontDescriptor)
            typeface = registeredFont?.typeface
            fontLoader = registeredFont?.loader
        }

        if (typeface != null) {
            return typeface
        }

        if (fontLoader != null && fontLoader.allowSynchronousLoad()) {
            return doLoadSync(fontDescriptor)
        }

        return null
    }

    /**
     * Synchronously get a Typeface, loads it if necessary.
     * This should never be called from the main thread.
     */
    fun loadSynchronously(fontDescriptor: FontDescriptor): Typeface {
        val typeface = get(fontDescriptor)
        if (typeface != null) {
            return typeface
        }

        return doLoadSync(fontDescriptor)
    }

    fun registerModuleTypefaceDataProvider(moduleTypefaceDataProvider: ModuleTypefaceDataProvider) {
        synchronized (this) {
            moduleTypefaceDataProviders.add(moduleTypefaceDataProvider)
        }
    }

    fun unregisterModuleTypefaceDataProvider(moduleTypefaceDataProvider: ModuleTypefaceDataProvider) {
        synchronized (this) {
            moduleTypefaceDataProviders.remove(moduleTypefaceDataProvider)
        }
    }

    private fun doLoadSync(fontDescriptor: FontDescriptor): Typeface {
        val latch = CountDownLatch(1)

        var loadedTypeface: Typeface? = null
        var failure: Throwable? = null

        val loadCompletion = object: LoadCompletion<Typeface> {
            override fun onSuccess(item: Typeface) {
                loadedTypeface = item
                latch.countDown()
            }

            override fun onFailure(error: Throwable) {
                failure = error
                latch.countDown()
            }
        }

        load(fontDescriptor, loadCompletion)

        latch.await()

        val typeface = loadedTypeface

        if (typeface != null) {
            return typeface
        }

        if (failure != null) {
            throw failure!!
        }

        throw ValdiException("Completion handler not called after Font load completed")
    }

    /**
     * Asynchronously load the Typeface given the FontDescriptor. Calls the completion once done.
     */
    fun load(fontDescriptor: FontDescriptor, completion: LoadCompletion<Typeface>): Disposable? {
        var alreadyLoadedTypeface: Typeface? = null
        var error: Throwable? = null
        var disposable: Disposable? = null

        synchronized(this) {
            val registeredFont  = resolveFont(fontDescriptor)
            if (registeredFont != null) {
                if (registeredFont.typeface != null) {
                    alreadyLoadedTypeface = registeredFont.typeface
                } else {
                    disposable = loader.load(FontLoadOperation(fontDescriptor, registeredFont.loader!!), completion)
                }
            } else {
                error = ValdiException("No FontLoader registered for font descriptor $fontDescriptor")
            }
        }

        if (alreadyLoadedTypeface != null) {
            completion.onSuccess(alreadyLoadedTypeface!!)
        } else if (error != null) {
            completion.onFailure(error!!)
        }
        return disposable
    }

    /**
     * Asynchronously preload all the registered fonts
     */
    fun preloadAll() {
        synchronized(this) {
            registeredFontsByName.values.forEach(this::preloadRegisteredFontIfNeeded)
            registeredFontsByFamily.values.forEach { it.forEach(this::preloadRegisteredFontIfNeeded) }
        }
    }

    private fun preloadRegisteredFontIfNeeded(registeredFont: RegisteredFont) {
        if (registeredFont.typeface != null) {
            return
        }

        val fontLoader = registeredFont.loader ?: return

        loader.load(FontLoadOperation(registeredFont.descriptor, fontLoader), object: LoadCompletion<Typeface> {
            override fun onSuccess(item: Typeface) {}
            override fun onFailure(error: Throwable) {}
        })
    }

}
