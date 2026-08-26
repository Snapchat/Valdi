package com.snap.valdi.support

import android.content.Intent
import android.content.pm.ApplicationInfo
import android.util.Log
import com.snap.valdi.views.ValdiRootView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLog

class TestAppBootstrapActivity : AppBootstrapActivity() {
    val lifecycleEvents = mutableListOf<String>()

    override fun createRootView(bootstrapper: AppBootstrapper): ValdiRootView {
        error("Runtime bootstrap is replaced by the test seam")
    }

    protected override fun loadNativeLibrary() {
        lifecycleEvents += "loadNativeLibrary"
    }

    protected override fun setDebuggerPortEnvironment(debuggerPort: Int) {
        lifecycleEvents += "setDebuggerPortEnvironment:$debuggerPort"
    }

    protected override fun bootstrapValdiRuntime() {
        lifecycleEvents += "bootstrapValdiRuntime"
    }
}

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [19], manifest = Config.NONE)
class AppBootstrapActivityTest {

    @Before
    fun clearLogs() {
        ShadowLog.clear()
    }

    @Test
    fun debuggerPortOverrideIsAvailableOnlyToDebuggableApplications() {
        val intent = Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, 13644)
        val wrongTypedIntent = Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, "secret-port-value")

        assertEquals(13644, invokeRequestedValdiDebuggerPort(intent, true))
        assertNull(invokeRequestedValdiDebuggerPort(intent, false))
        assertNull(invokeRequestedValdiDebuggerPort(wrongTypedIntent, false))
        assertTrue(valdiWarnings().isEmpty())
    }

    @Test
    fun acceptsDebuggerPortBoundaries() {
        assertEquals(
            1,
            invokeRequestedValdiDebuggerPort(
                Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, 1),
                true,
            ),
        )
        assertEquals(
            65535,
            invokeRequestedValdiDebuggerPort(
                Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, 65535),
                true,
            ),
        )
    }

    @Test
    fun absentDebuggerPortOverrideDoesNotWarn() {
        assertNull(invokeRequestedValdiDebuggerPort(null, true))
        assertNull(invokeRequestedValdiDebuggerPort(Intent(), true))

        assertTrue(valdiWarnings().isEmpty())
    }

    @Test
    fun invalidDebuggerPortOverridesWarnOnceWithRedactedValues() {
        val invalidIntents = listOf(
            Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, -1),
            Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, 0),
            Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, 65536),
            Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, "secret-port-value"),
        )

        invalidIntents.forEach { intent ->
            ShadowLog.clear()

            assertNull(invokeRequestedValdiDebuggerPort(intent, true))

            val warnings = valdiWarnings()
            assertEquals(1, warnings.size)
            assertTrue(warnings.single().contains("<redacted>"))
            assertFalse(warnings.single().contains("secret-port-value"))
        }
    }

    @Test
    fun onCreateAppliesValidDebuggerPortAfterLibraryLoadOnApi19() {
        val activity = createActivity(
            Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, 13702),
            debuggable = true,
        )

        assertEquals(
            listOf("loadNativeLibrary", "setDebuggerPortEnvironment:13702", "bootstrapValdiRuntime"),
            activity.lifecycleEvents,
        )
        assertTrue(valdiWarnings().isEmpty())
    }

    @Test
    fun onCreateNeverAppliesAbsentInvalidWrongTypedOrNonDebuggableOverrides() {
        val cases = listOf(
            Triple(Intent(), true, 0),
            Triple(Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, -1), true, 1),
            Triple(Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, 65536), true, 1),
            Triple(Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, "secret-port-value"), true, 1),
            Triple(Intent().putExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA, 13702), false, 0),
        )

        cases.forEach { (intent, debuggable, expectedWarnings) ->
            ShadowLog.clear()

            val activity = createActivity(intent, debuggable)

            assertEquals(listOf("loadNativeLibrary", "bootstrapValdiRuntime"), activity.lifecycleEvents)
            assertEquals(expectedWarnings, valdiWarnings().size)
            assertFalse(valdiWarnings().any { it.contains("secret-port-value") })
        }
    }

    private fun valdiWarnings(): List<String> =
        ShadowLog.getLogsForTag("Valdi").filter { it.type == Log.WARN }.map { it.msg }

    private fun invokeRequestedValdiDebuggerPort(intent: Intent?, debuggable: Boolean): Int? =
        Class.forName("com.snap.valdi.support.AppBootstrapActivityKt")
            .getDeclaredMethod("requestedValdiDebuggerPort", Intent::class.java, Boolean::class.java)
            .invoke(null, intent, debuggable) as Int?

    private fun createActivity(intent: Intent, debuggable: Boolean): TestAppBootstrapActivity {
        val controller = Robolectric.buildActivity(TestAppBootstrapActivity::class.java, intent)
        val activity = controller.get()
        activity.setTheme(androidx.appcompat.R.style.Theme_AppCompat)
        activity.applicationInfo.flags = if (debuggable) {
            activity.applicationInfo.flags or ApplicationInfo.FLAG_DEBUGGABLE
        } else {
            activity.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE.inv()
        }
        controller.create()
        return activity
    }
}
