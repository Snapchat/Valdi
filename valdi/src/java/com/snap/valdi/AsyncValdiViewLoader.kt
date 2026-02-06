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
import com.snap.valdi.utils.IScheduler
import com.snap.valdi.utils.isMainThread
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.views.ValdiRootView
import com.snapchat.client.valdi_core.Asset
import com.snapchat.client.valdi_core.ModuleFactory
import com.snapchat.client.valdi.NativeBridge
import java.util.concurrent.CountDownLatch

open class AsyncValdiRuntime(
        override val context: Context,
        private val scheduler: IScheduler,
        private val factory: Lazy<IValdiRuntime>,
) : IValdiRuntime {

    private var isInitialized = false
    private var scheduledInitialization = false
    private var callbacks: ArrayDeque<(IValdiRuntime) -> Unit>? = null

    init {
        isInitialized = factory.isInitialized()
    }

    fun getRuntime(completion: (IValdiRuntime) -> Unit) {
        var shouldNotify = false

        synchronized(this) {
            if (!isInitialized) {
                var callbacks = this.callbacks
                if (callbacks == null) {
                    callbacks = ArrayDeque()
                    this.callbacks = callbacks
                }

                callbacks.add(completion)

                if (!scheduledInitialization) {
                    scheduledInitialization = true
                    scheduleLoadAndFlushCallbacks()
                }
            } else {
                shouldNotify = true
            }
        }

        if (shouldNotify) {
            completion(factory.value)
        }
    }

    private fun scheduleLoadAndFlushCallbacks() {
        scheduler.schedule {
            val value = factory.value

            while (true) {
                var nextCallback: (IValdiRuntime) -> Unit
                synchronized(this) {
                    val callbacks = this.callbacks
                    if (!callbacks.isNullOrEmpty()) {
                        nextCallback = callbacks.removeAt(0)
                    } else {
                        isInitialized = true
                        this.callbacks = null
                        return@schedule
                    }
                }

                nextCallback(value)
            }
        }
    }

    override fun createValdiContext(componentPath: String,
                                       viewModel: Any?,
                                       componentContext: Any?,
                                       owner: ValdiViewOwner?,
                                       configuration: ValdiContextConfiguration?,
                                       completion: (ValdiContext) -> Unit) {
        getRuntime { it.createValdiContext(componentPath, viewModel, componentContext, owner, configuration, completion) }
    }

    override fun inflateViewAsync(rootView: ValdiRootView,
                                  componentPath: String,
                                  viewModel: Any?,
                                  componentContext: Any?,
                                  owner: ValdiViewOwner?,
                                  completion: InflationCompletion?,
                                  configuration: ValdiContextConfiguration?) {
        getRuntime {
            it.inflateViewAsync(rootView, componentPath, viewModel, componentContext, owner, completion, configuration)
        }
    }

    override fun <T : View> registerGlobalAttributesBinder(attributesBinder: AttributesBinder<T>) {
        getRuntime {
            it.registerGlobalAttributesBinder(attributesBinder)
        }
    }

    override fun registerNativeModuleFactory(moduleFactory: ModuleFactory) {
        getRuntime {
            it.registerNativeModuleFactory(moduleFactory)
        }
    }

    override fun unloadAllJsModules() {
        getRuntime {
            it.unloadAllJsModules()
        }
    }

    override fun getJSRuntime(block: (ValdiJSRuntime) -> Unit) {
        getRuntime {
            it.getJSRuntime(block)
        }
    }

    override fun createScopedJSRuntime(block: (ValdiScopedJSRuntime) -> Unit) {
        createScopedJSRuntime("", block)
    }

    override fun createScopedJSRuntime(scopeName: String, block: (ValdiScopedJSRuntime) -> Unit) {
        getRuntime { it.createScopedJSRuntime(scopeName, block) }
    }

    override fun loadModule(moduleName: String, completion: (error: String?) -> Unit) {
        getRuntime {
            it.loadModule(moduleName, completion)
        }
    }

    override fun getFontManager(block: (FontManager) -> Unit) {
        getRuntime { it.getFontManager(block) }
    }

    override fun getAssetsManager(block: (AssetsManager) -> Unit) {
        getRuntime { it.getAssetsManager(block) }
    }

    override fun dumpLogMetadata(): String {
        return factory.value.dumpLogMetadata()
    }

    override fun dumpCrashLogMetadata(): List<Pair<String, String>> {
        return if (factory.isInitialized()) factory.value.dumpCrashLogMetadata() else emptyList()
    }

    override fun getNativeModules(block: (ValdiNativeModules) -> Unit) {
        getRuntime { it.getNativeModules(block) }
    }

    override fun getManager(block: (manager: ValdiRuntimeManager) -> Unit) {
        getRuntime { it.getManager(block) }
    }

    override fun startProfiling() {
        getRuntime { it.startProfiling() }
    }

    override fun stopProfiling(completion: (content: List<String>, error: String?) -> Unit) {
        getRuntime { it.stopProfiling(completion) }
    }

    override fun getManagerSync(): ValdiRuntimeManager {
        if (factory.isInitialized() || !isMainThread()) {
            return factory.value.getManagerSync()
        }

        // HACK(simon): Workaround to get the RuntimeManager creation always happen in the
        // provided executor. We should change the createViewFactory() call such that it
        // wouldn't require a concrete RuntimeManager instance.

        val latch = CountDownLatch(1)
        var runtimeManager: ValdiRuntimeManager? = null

        getManager {
            runtimeManager = it
            latch.countDown()
        }

        latch.await()

        return runtimeManager!!
    }

    override fun dispose() {
        getRuntime { it.dispose() }
    }
}
