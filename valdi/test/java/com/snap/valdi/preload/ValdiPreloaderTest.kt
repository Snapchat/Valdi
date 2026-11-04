package com.snap.valdi.preload

import com.snap.valdi.IValdiRuntime
import org.junit.Assert.assertEquals
import org.junit.Test
import java.lang.reflect.Proxy

class ValdiPreloaderTest {

    @Test
    fun preloadModule_labelsTheJsRuntimeDispatchWithTheModulePath() {
        val callsites = mutableListOf<String>()
        val runtime = Proxy.newProxyInstance(
            IValdiRuntime::class.java.classLoader,
            arrayOf(IValdiRuntime::class.java),
        ) { _, method, args ->
            if (method.name == "getJSRuntime" && args?.size == 2) {
                callsites.add(args[0] as String)
            }
            null
        } as IValdiRuntime

        ValdiPreloader(runtime).preloadModule("my_module/src/MyComponent", 1)

        assertEquals(listOf("preloadModule(my_module/src/MyComponent)"), callsites)
    }
}
