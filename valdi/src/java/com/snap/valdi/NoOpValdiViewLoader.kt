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

open class NoOpValdiRuntime(): IValdiRuntime {

    override val context: Context
        get() = TODO()

    override fun createValdiContext(componentPath: String,
                                       viewModel: Any?,
                                       componentContext: Any?,
                                       owner: ValdiViewOwner?,
                                       configuration: ValdiContextConfiguration?,
                                       completion: (ValdiContext) -> Unit) {}

    override fun inflateViewAsync(rootView: ValdiRootView,
                                  componentPath: String,
                                  viewModel: Any?,
                                  componentContext: Any?,
                                  owner: ValdiViewOwner?,
                                  completion: InflationCompletion?,
                                  configuration: ValdiContextConfiguration?) {}

    override fun <T : View> registerGlobalAttributesBinder(attributesBinder: AttributesBinder<T>) {}

    override fun registerNativeModuleFactory(moduleFactory: ModuleFactory) {}

    override fun unloadAllJsModules() {}

    override fun getJSRuntime(block: (ValdiJSRuntime) -> Unit) {}

    override fun createScopedJSRuntime(block: (ValdiScopedJSRuntime) -> Unit) {}

    override fun createScopedJSRuntime(scopeName: String, block: (ValdiScopedJSRuntime) -> Unit) {}

    override fun loadModule(moduleName: String, completion: (error: String?) -> Unit) {}

    override fun getFontManager(block: (FontManager) -> Unit) {}

    override fun getAssetsManager(block: (AssetsManager) -> Unit) {}

    override fun dumpLogMetadata(): String {
        return ""
    }

    override fun startProfiling() {}

    override fun stopProfiling(completion: (content: List<String>, error: String?) -> Unit) {}

    override fun getNativeModules(block: (ValdiNativeModules) -> Unit) {}

    override fun getManager(block: (manager: ValdiRuntimeManager) -> Unit) {}

    override fun getManagerSync(): ValdiRuntimeManager {
        TODO()
    }

    override fun dispose() {}
}
