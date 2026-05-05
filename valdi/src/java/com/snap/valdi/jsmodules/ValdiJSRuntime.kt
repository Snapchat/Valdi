package com.snap.valdi.jsmodules

import com.snap.valdi.utils.ValdiMarshaller
import com.snapchat.client.valdi_core.JSRuntime

interface ValdiJSRuntime: JSThreadDispatcher {

    fun pushModuleToMarshaller(modulePath: String, marshaller: ValdiMarshaller): Int

    /**
     * Observe hot reload for the module at the given path.
     * Calls the given callback when the module is hot reloaded.
     */
    fun addHotReloadObserver(modulePath: String, callback: Runnable)

    /**
     Preload the given module given as an absolute path (e.g. 'valdi_core/src/Renderer').
     When maxDepth is more than 1, the preload will apply recursively to modules that the given
     modulePath imports, up until the given depth.
     */
    fun preloadModule(modulePath: String, maxDepth: Int)

    /**
     * Batch-preload multiple modules in a single JNI/JS-thread dispatch.
     */
    fun preloadModules(modulePaths: List<String>, maxDepth: Int)

    /**
     * Return the native JSRuntime handle
     */
    fun getNativeObject(): JSRuntime;
}
