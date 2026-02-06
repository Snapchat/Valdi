package com.snap.valdi

import android.content.Context
import android.view.View
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.bundle.AssetsManager
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.context.ValdiViewOwner
import com.snap.valdi.context.IValdiContext
import com.snap.valdi.context.LazyValdiContext
import com.snap.valdi.jsmodules.ValdiJSRuntime
import com.snap.valdi.jsmodules.ValdiScopedJSRuntime
import com.snap.valdi.modules.ValdiNativeModules
import com.snap.valdi.utils.Disposable
import com.snap.valdi.utils.IAsyncWorkScheduler
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.views.ValdiRootView
import com.snapchat.client.valdi_core.ModuleFactory

interface IValdiRuntime: Disposable {

    val context: Context

    fun inflateViewAsync(rootView: ValdiRootView,
                         componentPath: String,
                         viewModel: Any?,
                         componentContext: Any?,
                         owner: ValdiViewOwner?,
                         completion: InflationCompletion?,
                         configuration: ValdiContextConfiguration? = null)

    /**
     * Create a ValdiContext without a root view using the given componentPath for the root component.
     * A root view can later be attached to the ValdiContext to inflate the view hierarchy
     */
    fun createValdiContext(componentPath: String,
                              viewModel: Any?,
                              componentContext: Any?,
                              owner: ValdiViewOwner?,
                              configuration: ValdiContextConfiguration?,
                              completion: (ValdiContext) -> Unit) {
        // no-op by default for backward compat
    }

    fun registerNativeModuleFactory(moduleFactory: ModuleFactory)

    fun unloadAllJsModules()

    fun <T: View> registerGlobalAttributesBinder(attributesBinder: AttributesBinder<T>)

    // DEPRECATED: DO NOT USE, use registerGlobalAttributesBinder instead (or consider using ValdiContext.registerAttributesBinder)
    fun <T: View> registerAttributesBinder(attributesBinder: AttributesBinder<T>) {
        registerGlobalAttributesBinder(attributesBinder)
    }

    // DEPRECATED: DO NOT USE, use createScopedJSRuntime
    fun getJSRuntime(block: (ValdiJSRuntime) -> Unit)

    /**
     * Creates a scoped JS runtime for executing TypeScript code.
     *
     * @param block Callback invoked with the created runtime.
     */
    fun createScopedJSRuntime(block: (ValdiScopedJSRuntime) -> Unit)

    /**
     * Creates a scoped JS runtime for executing TypeScript code.
     *
     * @param scopeName A descriptive name identifying where this scoped runtime is created from.
     *                  This name appears in error messages to help debug issues with disposed
     *                  references. Callers should provide a meaningful name (e.g., class name
     *                  or feature name) to make error messages actionable.
     * @param block Callback invoked with the created runtime.
     */
    fun createScopedJSRuntime(scopeName: String, block: (ValdiScopedJSRuntime) -> Unit) {
        // Default implementation for backward compatibility.
        // Implementations should override this method to use scopeName for attribution.
        createScopedJSRuntime(block)
    }

    fun getFontManager(block: (FontManager) -> Unit)

    fun getAssetsManager(block: (AssetsManager) -> Unit) {
        throw Exception("Not implemented")
    }

    fun loadModule(moduleName: String, completion: (error: String?) -> Unit)

    fun getNativeModules(block: (ValdiNativeModules) -> Unit)

    fun getManager(block: (manager: ValdiRuntimeManager) -> Unit)

    fun getManagerSync(): ValdiRuntimeManager {
        throw Exception("Not implemented")
    }

    fun dumpLogMetadata(): String

    fun dumpCrashLogMetadata(): List<Pair<String, String>> {
        return emptyList()
    }

    fun startProfiling() {}

    /**
     * Profiling result of main JavaScript thread is returned in the 0th index of the content list.
     * Javascript worker profiling results are returned in the subsequent indices.
     * The returned String is a json formatted Chrome DevTools profile log.
     */
    fun stopProfiling(completion: (content: List<String>, error: String?) -> Unit) {}

}

/**
 * @DEPRECATED Use the componentPath API instead.
 */
fun IValdiRuntime.loadUntypedView(bundleName: String, viewName: String, viewModel: Any? = null, componentContext: Any? = null, owner: ValdiViewOwner? = null, completion: InflationCompletion? = null): ValdiRootView {
    val componentPath = "$bundleName/$viewName"
    return loadUntypedView(componentPath, viewModel, componentContext, owner)
}

fun IValdiRuntime.loadUntypedView(componentPath: String, viewModel: Any? = null, componentContext: Any? = null, owner: ValdiViewOwner? = null, completion: InflationCompletion? = null): ValdiRootView {
    val view = ValdiRootView(this.context)
    inflateViewAsync(view, componentPath, viewModel, componentContext, owner, completion)
    return view
}

fun <T: View> IValdiRuntime.createViewFactory(viewClass: Class<T>, factory: (context: Context) -> T, attributesBinder: AttributesBinder<T>?): ViewFactory {
    val lazy = lazy { this.getManagerSync().createViewFactory(viewClass, factory, attributesBinder) }
    return LazyViewFactory(lazy)
}

/**
 * Create an IValdiContext instance that will only start rendering once a root view is attached, or once
 * waitUntilAllUpdatesCompleted() is called.
 * An IAsyncWorkScheduler instance can be passed in to regulate how load operations are processed.
 */
fun IValdiRuntime.createLazyValdiContext(schedulerAsync: IAsyncWorkScheduler?,
                                                  componentPath: String,
                                                  viewModel: Any?,
                                                  componentContext: Any?,
                                                  configuration: ValdiContextConfiguration? = null): IValdiContext {
    return LazyValdiContext(this, schedulerAsync, componentPath, viewModel, componentContext, configuration)
}
