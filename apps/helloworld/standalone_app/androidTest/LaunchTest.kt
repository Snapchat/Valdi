package com.snap.helloworld.test

import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.snap.helloworld.StartActivity
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Instrumentation test to verify exported library integration.
 */
@RunWith(AndroidJUnit4::class)
class LaunchTest {
    
    @get:Rule
    val activityRule = ActivityScenarioRule(StartActivity::class.java)

    @Test
    fun testAppLaunchesSuccessfully() {
        // Verify the app context is correct
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        assertEquals("com.snap.helloworld", appContext.packageName)
        
        // If we get here, the activity launched successfully and didn't crash
        // The ActivityScenarioRule would have failed if the activity crashed on launch
        activityRule.scenario.onActivity { activity ->
            // Verify the activity is in a valid state
            assert(!activity.isFinishing) { "Activity should not be finishing" }
            assert(!activity.isDestroyed) { "Activity should not be destroyed" }
        }
    }
    
    @Test
    fun testNativeLibraryLoaded() {
        activityRule.scenario.onActivity { activity ->
            // If we get here, it means System.loadLibrary("hello_world_export") succeeded
            // in StartActivity.onCreate() without throwing UnsatisfiedLinkError
            
            // Verify the activity is still alive (didn't crash from native library issues)
            assert(!activity.isFinishing) { "Activity should not be finishing after native lib load" }
            assert(!activity.isDestroyed) { "Activity should not be destroyed after native lib load" }
            
            // Verify runtime manager was created (requires native library to be loaded)
            assertNotNull("RuntimeManager should exist after native library load", activity.runtimeManager)
        }
    }
    
    @Test
    fun testValdiRuntimeInitialized() {
        activityRule.scenario.onActivity { activity ->
            // Verify the Valdi runtime manager exists and is initialized
            val runtimeManager = activity.runtimeManager
            assertNotNull("ValdiRuntimeManager should be initialized", runtimeManager)
            
            // If the main runtime is accessible, the Valdi runtime initialized successfully
            // This requires both the native library and the Valdi core module to be loaded
            val mainRuntime = runtimeManager?.mainRuntime
            assertNotNull("Main Valdi runtime should be accessible", mainRuntime)
        }
    }
    
}

