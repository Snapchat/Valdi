package com.snap.valdi.callable

import com.snap.valdi.jsmodules.ValdiJSRuntime
import com.snap.valdi.schema.ValdiValueMarshallerRegistry
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.isMainThread

open class ValdiBridgeFunction {

    companion object {
        /**
         * When async_strict_mode is enabled, asserts that the current thread is not the main thread (to avoid ANRs).
         * Call this at the start of resolution (e.g. create()) to avoid generating the same check in every function class.
         */
        @JvmStatic
        fun assertResolutionNotOnMainThreadIfNeeded(asyncStrictMode: Boolean) {
            if (asyncStrictMode) {
                check(!isMainThread()) {
                    "When async_strict_mode is enabled, function resolution (create) must not be called from the main thread (to avoid ANRs). Use a background thread, the JS thread, or invokeWithJSRuntime."
                }
            }
        }

        @JvmStatic
        fun <T: ValdiBridgeFunction> createFromRuntime(runtime: ValdiJSRuntime,
                                                          cls: Class<T>,
                                                          modulePath: String): T {
            return ValdiMarshaller.use {
                val registry = ValdiValueMarshallerRegistry.shared
                registry.setActiveSchemaOfClassToMarshaller(cls, it)
                val objectIndex = runtime.pushModuleToMarshaller(modulePath, it)
                it.checkError()
                ValdiValueMarshallerRegistry.shared.unmarshallObject(cls, it, objectIndex)
            }
        }
    }

}
