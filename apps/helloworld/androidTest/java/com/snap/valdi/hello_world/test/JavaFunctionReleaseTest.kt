package com.snap.valdi.hello_world.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.test.ValdiTestRule
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.views.ValdiRootView
import java.lang.ref.WeakReference
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Hermes sweeps old objects on its own background thread, which the JVM never attached.
 * Hands Kotlin functions to JS, lets the garbage collector free them, and checks every one
 * is released without the process crashing. The padding makes JS allocate enough for
 * that background collection to run.
 */
@RunWith(AndroidJUnit4::class)
class JavaFunctionReleaseTest {

    @get:Rule
    val valdi = ValdiTestRule { runtime ->
        ValdiRootView(runtime.context).also { rootView ->
            runtime.inflateViewAsync(rootView, COMPONENT_PATH, null, null, null, null)
        }
    }

    @Test
    fun releasesJavaFunctionsTheJsGarbageCollectorFrees() {
        valdi.waitForNextRender()

        val functions = List(FUNCTION_COUNT) { round -> handOverFunction(round) }
        render(emptyMap())

        var cycles = 0
        while (countAlive(functions) > 0 && cycles < MAX_COLLECTION_CYCLES) {
            valdi.runtimeManager.mainRuntime.performGcNow(synchronous = true)
            Runtime.getRuntime().gc()
            cycles += 1
        }

        assertEquals(0, countAlive(functions))
    }

    private fun countAlive(functions: List<WeakReference<NoOpFunction>>): Int =
        functions.count { it.get() != null }

    private fun handOverFunction(round: Int): WeakReference<NoOpFunction> {
        val function = NoOpFunction()
        render(mapOf("callback" to function, "padding" to padding(round)))
        return WeakReference(function)
    }

    private fun padding(round: Int): List<String> = List(PADDING_SIZE) { "$round-$it" }

    private fun render(viewModel: Map<String, Any>) {
        valdi.view.setViewModelUntyped(viewModel)
        valdi.waitForNextRender()
    }

    private class NoOpFunction : ValdiFunction {
        override fun perform(marshaller: ValdiMarshaller): Boolean = false
    }

    private companion object {
        const val COMPONENT_PATH = "App@hello_world/src/HelloWorldApp"
        const val FUNCTION_COUNT = 50
        const val MAX_COLLECTION_CYCLES = 10
        const val PADDING_SIZE = 50_000
    }
}
