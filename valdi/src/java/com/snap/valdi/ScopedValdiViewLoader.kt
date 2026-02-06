package com.snap.valdi

import android.content.Context
import android.view.View
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.bundle.AssetsManager
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.context.ValdiViewOwner
import com.snap.valdi.jsmodules.ValdiJSRuntime
import com.snap.valdi.jsmodules.ValdiScopedJSRuntime
import com.snap.valdi.modules.ValdiNativeModules
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.views.ValdiRootView
import com.snapchat.client.valdi_core.Asset
import com.snapchat.client.valdi_core.ModuleFactory

class ScopedValdiRuntime(private val runtime: IValdiRuntime,
                               private val valdiContextConfiguration: ValdiContextConfiguration?): IValdiRuntime {

    override val context: Context
        get() = runtime.context

    private val localAttributesBinders = mutableListOf<AttributesBinder<View>>()
    private val localViewFactories = mutableListOf<Triple<Class<View>, (Context) -> View, AttributesBinder<View>?>>()

    private fun setupContext(context: ValdiContext) {
        synchronized(localAttributesBinders) {
            localAttributesBinders.forEach { context.registerAttributesBinder(it) }
        }
        synchronized(localViewFactories) {
            localViewFactories.forEach {
                val (viewClass, factory, attributesBinder) = it
                context.registerViewFactory(viewClass, factory, attributesBinder)
            }
        }
    }

    /**
     * Registers an attributes binder which will be locally registered to all new root views inflated
     * from this ScopedRuntime.
     *
     * Note: this has to be done before the views are inflated.
     */
    fun <T : View> registerLocalAttributesBinder(attributesBinder: AttributesBinder<T>) {
        synchronized(localAttributesBinders) {
            localAttributesBinders.add(attributesBinder as AttributesBinder<View>)
        }
    }

    /**
     * Register a custom view factory for the given view class which will be locally registered to all new root views inflated from this ScopedRuntime.
     * The created views from this factory will be enqueued into a local view pool instead of the global view pool.
     * If provided, the attributesBinder callback will be used to bind any additional attributes.
     *
     * Note: this has to be done before the views are inflated.
     */
    fun <T : View> registerLocalViewFactory(viewClass: Class<T>, factory: (context: Context) -> T, attributesBinder: AttributesBinder<T>?) {
        synchronized(localViewFactories) {
            localViewFactories.add(Triple(viewClass as Class<View>, factory as ((context: Context) -> T), attributesBinder as AttributesBinder<View>?))
        }
    }

    private fun resolveConfiguration(configuration: ValdiContextConfiguration?): ValdiContextConfiguration {
        val prepareContext: (ValdiContext) -> Unit = {
            this.setupContext(it)
            valdiContextConfiguration?.prepareContext?.invoke(it)
            configuration?.prepareContext?.invoke(it)
        }

        return ValdiContextConfiguration(
                valdiContextConfiguration?.renderBackend ?: configuration?.renderBackend,
                valdiContextConfiguration?.snapDrawingOptions ?: configuration?.snapDrawingOptions,
                valdiContextConfiguration?.useLegacyMeasureBehavior ?: configuration?.useLegacyMeasureBehavior,
                valdiContextConfiguration?.preloadingMode ?: configuration?.preloadingMode,
                prepareContext,
        )
    }

    override fun createValdiContext(componentPath: String,
                                       viewModel: Any?, componentContext
                                       : Any?,
                                       owner: ValdiViewOwner?,
                                       configuration: ValdiContextConfiguration?,
                                       completion: (ValdiContext) -> Unit) {
        val resolvedConfiguration = resolveConfiguration(configuration)

        runtime.createValdiContext(componentPath,
                viewModel,
                componentContext,
                owner,
                resolvedConfiguration, completion)
    }

    override fun inflateViewAsync(rootView: ValdiRootView,
                                  componentPath: String,
                                  viewModel: Any?,
                                  componentContext: Any?,
                                  owner: ValdiViewOwner?,
                                  completion: InflationCompletion?,
                                  configuration: ValdiContextConfiguration?) {
        val resolvedConfiguration = resolveConfiguration(configuration)

        runtime.inflateViewAsync(rootView,
                componentPath,
                viewModel,
                componentContext,
                owner,
                completion,
                resolvedConfiguration)
    }

    override fun registerNativeModuleFactory(moduleFactory: ModuleFactory) {
        runtime.registerNativeModuleFactory(moduleFactory)
    }

    override fun unloadAllJsModules() {
        runtime.unloadAllJsModules()
    }

    override fun <T : View> registerGlobalAttributesBinder(attributesBinder: AttributesBinder<T>) {
        runtime.registerGlobalAttributesBinder(attributesBinder)
    }

    override fun getJSRuntime(block: (ValdiJSRuntime) -> Unit) {
        runtime.getJSRuntime(block)
    }

    override fun createScopedJSRuntime(block: (ValdiScopedJSRuntime) -> Unit) {
        createScopedJSRuntime("", block)
    }

    override fun createScopedJSRuntime(scopeName: String, block: (ValdiScopedJSRuntime) -> Unit) {
        runtime.createScopedJSRuntime(scopeName, block)
    }

    override fun getFontManager(block: (FontManager) -> Unit) {
        runtime.getFontManager(block)
    }

    override fun getAssetsManager(block: (AssetsManager) -> Unit) {
        runtime.getAssetsManager(block)
    }

    override fun loadModule(moduleName: String, completion: (error: String?) -> Unit) {
        runtime.loadModule(moduleName, completion)
    }

    override fun getNativeModules(block: (ValdiNativeModules) -> Unit) {
        runtime.getNativeModules(block)
    }

    override fun dumpLogMetadata(): String {
        return runtime.dumpLogMetadata()
    }

    override fun getManager(block: (manager: ValdiRuntimeManager) -> Unit) {
        runtime.getManager(block)
    }

    override fun getManagerSync(): ValdiRuntimeManager {
        return runtime.getManagerSync()
    }

    override fun dispose() {
        runtime.dispose()
    }

    override fun dumpCrashLogMetadata(): List<Pair<String, String>> {
        return runtime.dumpCrashLogMetadata()
    }

    override fun startProfiling() {
        runtime.startProfiling()
    }

    override fun stopProfiling(completion: (content: List<String>, error: String?) -> Unit) {
        runtime.stopProfiling(completion)
    }
}

/**
 * Returns a new ScopedValdiRuntime from this IValdiRuntime.
 */
fun IValdiRuntime.makeScoped(configuration: ValdiContextConfiguration? = null): ScopedValdiRuntime {
    return ScopedValdiRuntime(this, configuration)
}
