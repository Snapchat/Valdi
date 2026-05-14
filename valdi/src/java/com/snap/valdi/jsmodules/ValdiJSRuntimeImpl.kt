package com.snap.valdi.jsmodules

import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.ValdiMarshaller
import com.snapchat.client.valdi_core.JSRuntime
import com.snapchat.client.valdi_core.JSRuntimeNativeObjectsManager

import java.lang.ref.WeakReference

class ValdiJSRuntimeImpl(val jsRuntime: JSRuntime,
                            val jsThreadDispatcher: JSThreadDispatcher,
                            var nativeObjectsManager: JSRuntimeNativeObjectsManager?): ValdiScopedJSRuntime {

    override fun pushModuleToMarshaller(modulePath: String, marshaller: ValdiMarshaller): Int {
        val objectIndex = jsRuntime.pushModuleToMarshaller(nativeObjectsManager, modulePath, marshaller.nativeHandle)
        marshaller.checkError()
        return objectIndex
    }

    override fun addHotReloadObserver(modulePath: String, callback: Runnable) {
        jsRuntime.addModuleUnloadObserver(modulePath, object: ValdiFunction {
            override fun perform(marshaller: ValdiMarshaller): Boolean {
                callback.run()
                return false
            }
        })
    }

    override fun preloadModule(modulePath: String, maxDepth: Int) {
        jsRuntime.preloadModule(modulePath, maxDepth)
    }

    override fun preloadModules(modulePaths: List<String>, maxDepth: Int) {
        jsRuntime.preloadModules(ArrayList(modulePaths), maxDepth)
    }

    override fun runOnJsThread(runnable: Runnable) {
        return jsThreadDispatcher.runOnJsThread(runnable)
    }

    override fun getNativeObject(): JSRuntime {
        return jsRuntime
    }
    
    override fun dispose() {
        synchronized(this) {
            if (nativeObjectsManager != null) {
                jsRuntime.destroyNativeObjectsManager(nativeObjectsManager)
            }
        }
    }
}
