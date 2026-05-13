package com.snap.valdi.jsmodules

import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.ValdiMarshaller
import com.snapchat.client.valdi_core.JSRuntime

// User code should not create workers directly, instead, use ValdiRuntimeManager.getWorker() to acquire a worker
class ValdiJSWorker(val jsRuntime: JSRuntime) : ValdiJSRuntime {

    override fun pushModuleToMarshaller(modulePath: String, marshaller: ValdiMarshaller): Int {
        val objectIndex = jsRuntime.pushModuleToMarshaller(null/*jsRuntime.createNativeObjectsManager()?*/, modulePath, marshaller.nativeHandle)
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
        jsRuntime.runOnJsThread(object: ValdiFunction {
            override fun perform(marshaller: ValdiMarshaller): Boolean {
                runnable.run()
                return false
            }
        });
    }

    override fun getNativeObject(): JSRuntime {
        return jsRuntime
    }
}
