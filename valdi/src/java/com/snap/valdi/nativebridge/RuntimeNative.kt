package com.snap.valdi.nativebridge

import com.snapchat.client.valdi.NativeBridge
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.utils.BitmapPool
import com.snapchat.client.valdi_core.JSRuntime
import com.snapchat.client.valdi_core.ModuleFactory
import com.snapchat.client.valdi.utils.NativeHandleWrapper
import java.lang.Runnable

/**
 * Bridge class for a native Runtime, which forwards calls to the NativeBridge.
 */
class RuntimeNative(private val nativeBridge: NativeBridge, nativeHandle: Long): NativeHandleWrapper(nativeHandle) {

    override protected fun destroyHandle(handle: Long) {
        NativeBridge.deleteRuntime(handle)
    }

    fun registerNativeModuleFactory(moduleFactory: ModuleFactory) {
        NativeBridge.registerNativeModuleFactory(nativeHandle, moduleFactory)
    }

    fun updateResource(resource: ByteArray, bundleName: String, filePathWithinBundle: String) {
        NativeBridge.updateResource(nativeHandle, resource, bundleName, filePathWithinBundle)
    }

    fun callOnJsThread(sync: Boolean, runnable: Runnable) {
        NativeBridge.callOnJsThread(nativeHandle, sync, runnable)
    }

    fun unloadAllJsModules() {
        NativeBridge.unloadAllJsModules(nativeHandle)
    }

    fun performGcNow() {
        NativeBridge.performGcNow(nativeHandle)
    }

    fun createContext(componentPath: String, viewModel: Any?, componentContext: Any?, useSkia: Boolean): ValdiContext {
        return NativeBridge.createContext(nativeHandle, componentPath, viewModel, componentContext, useSkia) as ValdiContext
    }

    fun getJSRuntime(): JSRuntime {
        return NativeBridge.getJSRuntime(nativeHandle) as JSRuntime
    }

}
