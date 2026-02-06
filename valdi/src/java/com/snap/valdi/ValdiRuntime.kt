package com.snap.valdi

import android.content.Context
import android.view.View
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.context.ValdiViewOwner
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.jsmodules.ValdiJSRuntimeImpl
import com.snap.valdi.jsmodules.ValdiJSRuntime
import com.snap.valdi.jsmodules.ValdiScopedJSRuntime
import com.snap.valdi.logger.Logger
import com.snap.valdi.jsmodules.JSThreadDispatcher
import com.snap.valdi.modules.ValdiNativeModules
import com.snap.valdi.nativebridge.RuntimeNative
import com.snap.valdi.utils.ValdiMarshallable
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.JavaScriptCapturedStacktrace
import com.snap.valdi.utils.JavaScriptThreadStatus
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.bundle.AssetsManager
import com.snap.valdi.context.AndroidRootViewHandler
import com.snap.valdi.context.SnapDrawingRootViewHandler
import com.snap.valdi.snapdrawing.SnapDrawingOptions
import com.snap.valdi.utils.ViewRefSupport
import com.snap.valdi.utils.runOnMainThreadDelayed
import com.snap.valdi.utils.runOnMainThreadIfNeeded
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.views.ValdiRootView

import java.lang.ref.WeakReference

import com.snapchat.client.valdi_core.Asset
import com.snapchat.client.valdi_core.ModuleFactory
import com.snapchat.client.valdi.NativeBridge

/**
 * The Runtime is one of the core class of Valdi. It is primarily used to create a
 * View tree for its document name. Because it is backed by a C++ instance, destroy()
 * should be called when the Runtime is not needed to release the native resources.
 * The Runtime is stateful and holds a cache. Because of this, you should typically only
 * have a single instance per app. This is especially true if hot reloading is enabled, because
 * it uses a socket on a specific port. There can only be one Runtime alive with hotReloading
 * enabled at all time.
 */
class ValdiRuntime(
        val native: RuntimeNative,
        override val context: Context,
        val logger: Logger,
        var manager: ValdiRuntimeManager?,
        val viewRefSupport: ViewRefSupport,
        val isMain: Boolean
) : JSThreadDispatcher, MainThreadBatchDispatcher, IValdiRuntime, AssetsManager, FontManager.ModuleTypefaceDataProvider {

    var performGcOnContextDestroy = false

    private var cachingJSRuntime: ValdiJSRuntime? = null

    var nativeModules: ValdiNativeModules? = null

   init {
    // Used to verify that Valdi objects are correctly collected.
    //        scheduleGc()

        manager?.fontManager?.registerModuleTypefaceDataProvider(this)
   }


    private fun scheduleGc() {
        runOnMainThreadDelayed(5000L) {
            println("[valdi] GC!")
            performGcNow(true)
            Runtime.getRuntime().gc()
            scheduleGc()
        }
    }

    /**
     * Destroy the Runtime and release all its native resources.
     */
    fun destroy() {
        nativeModules?.dispose()

        if (native.nativeHandle != 0L) {
            NativeBridge.setRuntimeAttachedObject(native.nativeHandle, null)
            native.destroy()
        }

        manager?.fontManager?.unregisterModuleTypefaceDataProvider(this)
        manager = null
    }

    override fun dispose() {
        destroy()
    }

    private fun shouldEnableSnapDrawingRenderBackend(requestedRenderBackend: RenderBackend): Boolean {
        val manager = this.manager ?: return false

        return manager.shouldUseSnapDrawingRenderBackend(requestedRenderBackend)
    }

    private fun shouldEnableLegacyMeasureBehavior(useLegacyMeasureBehavior: Boolean?): Boolean {
        if (useLegacyMeasureBehavior != null) {
            return useLegacyMeasureBehavior
        }
        val manager = this.manager ?: return false

        return manager.useLegacyMeasureBehaviorByDefault
    }

    private fun doCreateValdiContext(componentPath: String,
                                        viewModel: Any?,
                                        componentContext: Any?,
                                        owner: ValdiViewOwner?,
                                        configuration: ValdiContextConfiguration?): ValdiContext {
        val renderBackend = configuration?.renderBackend ?: RenderBackend.DEFAULT
        val snapDrawingOptions = configuration?.snapDrawingOptions ?: SnapDrawingOptions()
        val preloadingMode = configuration?.preloadingMode ?: PreloadingMode.AGGRESSIVE

        manager?.prepareRenderBackend(renderBackend, preloadingMode)

        val useSnapDrawing = shouldEnableSnapDrawingRenderBackend(renderBackend)
        val context = native.createContext(componentPath,
                ValdiMarshallable.from(viewModel),
                ValdiMarshallable.from(componentContext),
                useSnapDrawing)
        context.usesSnapDrawingRenderBackend = useSnapDrawing
        context.useLegacyMeasureBehavior = shouldEnableLegacyMeasureBehavior(configuration?.useLegacyMeasureBehavior)

        if (useSnapDrawing) {
            context.rootViewHandler = SnapDrawingRootViewHandler(context.native,
                    snapDrawingOptions,
                    manager!!.snapDrawingRuntime!!,
                    viewRefSupport)
        } else {
            context.rootViewHandler = AndroidRootViewHandler(context.native, viewRefSupport)
        }

        context.setViewModelNoUpdate(viewModel)
        context.owner = owner

        if (componentContext != null) {
            context.componentContext = WeakReference(componentContext)
        }

        configuration?.prepareContext?.invoke(context)

        return context
    }

    override fun createValdiContext(componentPath: String,
                                       viewModel: Any?,
                                       componentContext: Any?,
                                       owner: ValdiViewOwner?,
                                       configuration: ValdiContextConfiguration?,
                                       completion: (ValdiContext) -> Unit) {
        val context = doCreateValdiContext(componentPath, viewModel, componentContext, owner, configuration)
        context.onCreate()
        runOnMainThreadIfNeeded {
            completion(context)
        }
    }

    override fun inflateViewAsync(rootView: ValdiRootView,
                                  componentPath: String,
                                  viewModel: Any?,
                                  componentContext: Any?,
                                  owner: ValdiViewOwner?,
                                  completion: InflationCompletion?,
                                  configuration: ValdiContextConfiguration?) {
        val context = doCreateValdiContext(componentPath, viewModel, componentContext, owner, configuration)

        runOnMainThreadIfNeeded {
            if (rootView.destroyed) {
                // destroyed was called before the completion was called
                context.destroy()
                return@runOnMainThreadIfNeeded
            }

            rootView.destroyValdiContextOnFinalize = true
            context.rootView = rootView

            if (completion != null) {
                context.enqueueNextRenderCallback { completion(null) }
            }
        }

        context.onCreate()
    }

    fun updateResource(resource: ByteArray, bundleName: String, filePathWithinBundle: String) {
        native.updateResource(resource, bundleName, filePathWithinBundle)
    }

    override fun executeMainThreadBatch(runnable: Runnable) {
        NativeBridge.callSyncWithJsThread(native.nativeHandle, runnable)
    }

    fun captureJavaScriptStackTraces(timeoutMs :Long) : List<JavaScriptCapturedStacktrace> {
        val stackTraces = NativeBridge.captureJavaScriptStackTraces(native.nativeHandle, timeoutMs) as Array<*>
        return stackTraces.map {
            val trace = it as Array<*>
            JavaScriptCapturedStacktrace(trace[0] as String, JavaScriptThreadStatus.fromCpp(trace[1] as Integer))
        }
    }

    /**
     * Gets a map of all (moduleName: hash) from C++ runtime
     * Converts the array retured by native (C++) call to a map
     * See BundleResourcesProcessor.swift for how the 'hash' is genereated for the Module.
     * @return A map of (moduleName: hash)
     */
    fun getAllModuleHashes(): Map<String, String> {
        val moduleHashes = NativeBridge.getAllModuleHashes(native.nativeHandle) as Array<*>
        return moduleHashes.associate {
            val moduleNameAndHash = it as Array<*>
            val moduleName = moduleNameAndHash[0] as String
            val hash = moduleNameAndHash[1] as String
            moduleName to hash
        }
    }

    inline fun runOnJsThread(crossinline callback: () -> Unit) {
        runOnJsThread(Runnable { callback() })
    }

    override fun runOnJsThread(runnable: Runnable) {
        native.callOnJsThread(false, runnable)
    }

    override fun registerNativeModuleFactory(moduleFactory: ModuleFactory) {
        native.registerNativeModuleFactory(moduleFactory)
    }

    override fun unloadAllJsModules() {
        native.unloadAllJsModules()
    }

    override fun <T : View> registerGlobalAttributesBinder(attributesBinder: AttributesBinder<T>) {
        manager?.registerGlobalAttributesBinder(attributesBinder)
    }

    override fun getJSRuntime(block: (ValdiJSRuntime) -> Unit) {
        runOnJsThread {
            if (cachingJSRuntime != null) {
                block(cachingJSRuntime!!)
                return@runOnJsThread
            }

            cachingJSRuntime = ValdiJSRuntimeImpl(native.getJSRuntime(), this, null)
            block(cachingJSRuntime!!)
        }
    }

    override fun createScopedJSRuntime(block: (ValdiScopedJSRuntime) -> Unit) {
        createScopedJSRuntime("", block)
    }

    override fun createScopedJSRuntime(scopeName: String, block: (ValdiScopedJSRuntime) -> Unit) {
        runOnJsThread {
            val jsRuntime = native.getJSRuntime()
            val scopedRuntime = ValdiJSRuntimeImpl(jsRuntime, this, jsRuntime.createNativeObjectsManager(scopeName))
            block(scopedRuntime)
        }
    }

    fun performGcNow(synchronous: Boolean) {
        native.performGcNow()
        if (synchronous) {
            native.callOnJsThread(true, object: Runnable {
                override fun run() { }
            })
        }
    }

    override fun loadModule(moduleName: String, completion: (error: String?) -> Unit) {
        NativeBridge.loadModule(native.nativeHandle, moduleName, object: ValdiFunction {
            override fun perform(marshaller: ValdiMarshaller): Boolean {
                val error = marshaller.getOptionalError(0)
                completion(error)
                return false
            }
        })
    }

    override fun getFontManager(block: (FontManager) -> Unit) {
        val manager = this.manager ?: return
        block(manager.fontManager)
    }

    override fun getAssetsManager(block: (AssetsManager) -> Unit) {
        block(this)
    }

    override fun dumpLogMetadata(): String {
        return NativeBridge.dumpLogMetadata(native.nativeHandle, false)
    }

    override fun dumpCrashLogMetadata(): List<Pair<String, String>> {
        val metadata = NativeBridge.dumpLogMetadata(native.nativeHandle, true)
        val lastMeasuredText = TextViewHelper.lastMeasuredText?.toString() ?: ""
        val lastMeasuredFontAttributes = TextViewHelper.lastMeasuredFontAttributes?.toString() ?: ""

        return listOf(
                "VALDI_METADATA" to metadata,
                "VALDI_LAST_MEASURED_TEXT" to lastMeasuredText,
                "VALDI_LAST_MEASURED_FONT_ATTRIBUTES" to lastMeasuredFontAttributes
        )
    }

    override fun startProfiling() {
        NativeBridge.startProfiling(native.nativeHandle)
    }

    override fun stopProfiling(completion: (content: List<String>, error: String?) -> Unit) {
        NativeBridge.stopProfiling(native.nativeHandle, object: ValdiFunction {
            override fun perform(marshaller: ValdiMarshaller): Boolean {
                val content = marshaller.getListOfStrings(0)
                val error = marshaller.getOptionalString(1)
                completion(content, error)
                return true
            }
        })
    }

    fun dumpLogs(): String {
        return NativeBridge.dumpLogs(native.nativeHandle)
    }

    fun getAllContexts(): List<ValdiContext> {
        return (NativeBridge.getAllContexts(native.nativeHandle) as Array<*>).map { it as ValdiContext }
    }

    override fun getNativeModules(block: (ValdiNativeModules) -> Unit) {
        block(nativeModules!!)
    }

    override fun getManager(block: (manager: ValdiRuntimeManager) -> Unit) {
        val manager = this.manager ?: return
        block(manager)
    }

    override fun getManagerSync(): ValdiRuntimeManager {
        return this.manager ?: throw ValdiException("ValdiRuntimeManager not set!")
    }

    override fun getLocalAsset(moduleName: String, path: String): Asset {
        return NativeBridge.getAsset(native.nativeHandle, moduleName, path) as Asset
    }

    override fun getUrlAsset(url: String): Asset {
        return NativeBridge.getAsset(native.nativeHandle, null, url) as Asset
    }

    override fun getModuleTypefacePath(moduleName: String, path: String): String? {
        return NativeBridge.getModuleEntry(native.nativeHandle, moduleName, "res/${path}.ttf", true) as? String
    }
}
