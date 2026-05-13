package com.snap.valdi.jsmodules

import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import com.snap.valdi.jsmodules.ValdiJSRuntime
import com.snap.valdi.utils.ValdiMarshaller
import com.snapchat.client.valdi_core.JSRuntime
import com.snap.modules.valdi_test.MakeTestObject
import com.snap.modules.valdi_test.ITestObject
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * Test for the generated invokeWithJSRuntime static method on @ExportFunction classes.
 * 
 * This test validates that the Kotlin code generator correctly produces:
 * - A static invokeWithJSRuntime method
 * - That takes a jsRuntimeProvider function
 * - Followed by the original function parameters
 * - And a completionHandler callback
 * - That dispatches to the JS thread
 * 
 * This test uses the actual generated MakeTestObject from FunctionTest.ts.
 * 
 * NOTE: These are compilation and structure tests using a mock runtime.
 * They verify the generated Kotlin code compiles and has the correct structure,
 * but do NOT test actual JavaScript execution, which requires a real Valdi runtime.
 * For full integration testing with JavaScript execution, use Android instrumentation tests.
 */
internal class InvokeWithJSRuntimeTest {

    /**
     * Mock JSRuntime that captures the runOnJsThread callback.
     * 
     * This mock does NOT execute JavaScript code - it only verifies the structure
     * of the generated Kotlin code. The actual JavaScript function execution
     * requires a real Valdi runtime with a JS engine.
     */
    private class MockValdiJSRuntime : ValdiJSRuntime {
        var capturedCallback: Runnable? = null
        var moduleWasPushed = false
        var pushModulePath: String? = null
        
        override fun pushModuleToMarshaller(modulePath: String, marshaller: ValdiMarshaller): Int {
            // Record that module was pushed, but don't actually execute JS
            moduleWasPushed = true
            pushModulePath = modulePath
            return 1
        }
        
        override fun addHotReloadObserver(modulePath: String, callback: Runnable) {
            // No-op for testing
        }
        
        override fun preloadModule(modulePath: String, maxDepth: Int) {
            // No-op for testing
        }

        override fun preloadModules(modulePaths: List<String>, maxDepth: Int) {
            // No-op for testing
        }
        
        override fun runOnJsThread(runnable: Runnable) {
            // Capture the callback - DON'T execute it since we can't run JS
            capturedCallback = runnable
        }
        
        override fun getNativeObject(): JSRuntime {
            throw UnsupportedOperationException("Not used in test")
        }
    }
    
    @Test
    fun testInvokeWithJSRuntimeGeneratedMethodExists() {
        // This test verifies that the generated MakeTestObject class has the correct
        // invokeWithJSRuntime method that compiles and can be called.
        // Expected signature:
        // public static fun invokeWithJSRuntime(
        //     jsRuntimeProvider: () -> ValdiJSRuntime,
        //     completionHandler: (ITestObject) -> Unit
        // )
        
        val mockRuntime = MockValdiJSRuntime()
        var runtimeProviderCalled = false
        
        // Call the actual generated invokeWithJSRuntime method
        // The fact that this compiles proves the method exists with correct signature
        MakeTestObject.invokeWithJSRuntime(
            jsRuntimeProvider = {
                runtimeProviderCalled = true
                mockRuntime
            },
            completionHandler = { _: ITestObject ->
                // This won't be called without a real JS runtime
            }
        )
        
        // Verify that the runtime provider was called
        assertTrue(runtimeProviderCalled, 
            "Runtime provider should be called when invokeWithJSRuntime is invoked")
        
        // Verify that runOnJsThread was invoked
        assertNotNull(mockRuntime.capturedCallback, 
            "invokeWithJSRuntime should dispatch to JS thread via runOnJsThread")
    }
    
    @Test
    fun testInvokeWithJSRuntimeDispatchesToJSThread() {
        // Verify that the generated method correctly dispatches to the JS thread
        
        val mockRuntime = MockValdiJSRuntime()
        
        // Call the generated method
        MakeTestObject.invokeWithJSRuntime(
            jsRuntimeProvider = { mockRuntime },
            completionHandler = { _ ->
                // This lambda would be called with the actual ITestObject
                // if we had a real JS runtime
            }
        )
        
        // Verify that runOnJsThread was called
        assertNotNull(mockRuntime.capturedCallback, 
            "invokeWithJSRuntime should dispatch work to the JS thread via runOnJsThread")
        
        // The actual JavaScript execution and completionHandler callback would happen
        // when capturedCallback.run() is called on the real JS thread
    }
    
    @Test
    fun testInvokeWithJSRuntimeCompletionHasCorrectSignature() {
        // Verify that the completionHandler callback has the correct type signature.
        // This test ensures the generated code accepts a completionHandler of type (ITestObject) -> Unit
        
        val mockRuntime = MockValdiJSRuntime()
        
        // Call the generated method with a strongly-typed completionHandler
        // The fact that this compiles without type errors proves the signature is correct
        MakeTestObject.invokeWithJSRuntime(
            jsRuntimeProvider = { mockRuntime },
            completionHandler = { _: ITestObject ->
                // Type checker validates that 'testObject' is ITestObject
                // In a real integration test with a JS runtime, we would verify:
                // - testObject.add(10.0) returns 10.0
                // - testObject.add(32.0) returns 42.0 (cumulative)
                
                // For this test, we just verify the code compiles
            }
        )
        
        // If we got here without compilation errors, the test passes
        assertTrue(true, 
            "Generated method accepts completionHandler: (ITestObject) -> Unit")
    }

    /**
     * valdi_test has async_strict_mode enabled. Resolving (create(runtime)) from the main thread
     * must throw IllegalStateException from ValdiBridgeFunction.assertResolutionNotOnMainThreadIfNeeded.
     */
    @Test
    fun testResolvingExportedFunctionOnMainThreadThrows() {
        val mockRuntime = MockValdiJSRuntime()
        // When run on the main thread, resolve must throw (main thread causes ANRs).
        assertThrows(IllegalStateException::class.java) {
            MakeTestObject.create(mockRuntime)
        }
    }
}
