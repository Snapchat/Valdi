package com.snap.valdi.test

import android.os.Looper
import android.view.View
import androidx.test.core.app.ActivityScenario
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import com.snap.valdi.IValdiRuntime
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.support.SupportValdiRuntimeManager
import com.snap.valdi.views.ValdiRootView
import org.junit.rules.TestRule
import org.junit.runner.Description
import org.junit.runners.model.Statement
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

/**
 * JUnit rule that mounts a single Valdi view in an instrumented test and tears it down again,
 * so the `com.snap.valdi.test` Espresso bindings ([ValdiEspresso], [ValdiElementMatchers],
 * [ValdiElementActions], [ValdiElementAssertions]) have something to point at.
 *
 * The rule launches [ValdiTestHostActivity], boots a Valdi runtime with
 * [SupportValdiRuntimeManager] and hands that runtime to [viewCreator], which returns the view
 * under test:
 *
 * ```kotlin
 * @get:Rule
 * val valdi = ValdiTestRule { runtime ->
 *     ValdiRootView(runtime.context).also {
 *         runtime.inflateViewAsync(it, "App@my_module/src/MyComponent", null, null, null, null)
 *     }
 * }
 *
 * @Test
 * fun rendersTitle() {
 *     valdi.waitForNextRender()
 *     ValdiEspresso.onValdiElement(ValdiElementMatchers.withText("Hello"))
 *         .check(ValdiElementAssertions.matches(ValdiElementMatchers.isDisplayed()))
 * }
 * ```
 *
 * The runtime manager is created once per process and shared by every rule instance, mirroring
 * the singleton-per-app guidance for production code — creating one per test method would pay
 * the full runtime + JS bundle startup cost on every method.
 *
 * @param nativeLibraryName name passed to [System.loadLibrary] before the runtime is created,
 *   matching the `so_name` of the app or exported library under test (`valdi` for apps built
 *   with `valdi_application`). Pass `null` when something else in the process has already
 *   loaded it.
 * @param viewCreator builds the view under test from the runtime. Called on the main thread.
 */
class ValdiTestRule<V : ValdiRootView> @JvmOverloads constructor(
    private val nativeLibraryName: String? = DEFAULT_NATIVE_LIBRARY_NAME,
    private val viewCreator: (IValdiRuntime) -> V
) : TestRule {

    private val hostActivity = AtomicReference<ValdiTestHostActivity?>()
    private val viewReference = AtomicReference<V?>()

    /** Process-wide runtime manager, created on first use. */
    val runtimeManager: ValdiRuntimeManager
        get() = onMainThread { sharedRuntimeManager(nativeLibraryName) }

    val runtime: IValdiRuntime
        get() = runtimeManager.mainRuntime

    /**
     * The view under test, mounted into the host activity on first access. Subsequent accesses
     * return the same view until [destroyView] is called.
     */
    val view: V
        get() {
            viewReference.get()?.let { return it }

            val activity = checkNotNull(hostActivity.get()) {
                "The host activity is not running. Is ValdiTestRule registered with @get:Rule?"
            }

            getInstrumentation().runOnMainSync {
                val created = viewCreator(runtime)
                created.id = View.generateViewId()
                activity.contentContainer.addView(created)
                viewReference.set(created)
            }

            return checkNotNull(viewReference.get())
        }

    /** Removes and destroys the view under test, if one was mounted. */
    fun destroyView() {
        getInstrumentation().runOnMainSync {
            val existingView = viewReference.getAndSet(null) ?: return@runOnMainSync
            hostActivity.get()?.contentContainer?.removeView(existingView)
            existingView.destroy()
        }
    }

    /**
     * Blocks until every pending Valdi update has been applied and the resulting layout pass has
     * run, then waits for the instrumentation to go idle. Call this before asserting on the view
     * tree — inflation is asynchronous.
     */
    @JvmOverloads
    fun waitForNextRender(timeoutMs: Long = DEFAULT_RENDER_TIMEOUT_MS) {
        // The latch is counted down from a main-thread layout callback, so awaiting it on the
        // main thread deadlocks until the timeout. Fail immediately instead.
        check(Looper.myLooper() != Looper.getMainLooper()) {
            "waitForNextRender must be called from the test thread, not the main thread"
        }

        val latch = CountDownLatch(1)

        view.getValdiContext { context ->
            context.waitUntilAllUpdatesCompleted {
                context.onNextLayout {
                    latch.countDown()
                }
            }
        }

        check(latch.await(timeoutMs, TimeUnit.MILLISECONDS)) {
            "Timed out after ${timeoutMs}ms waiting for the Valdi view to render"
        }

        getInstrumentation().waitForIdleSync()
    }

    override fun apply(base: Statement, description: Description): Statement {
        return object : Statement() {
            override fun evaluate() {
                ActivityScenario.launch(ValdiTestHostActivity::class.java).use { scenario ->
                    scenario.onActivity { hostActivity.set(it) }
                    try {
                        base.evaluate()
                    } finally {
                        destroyView()
                        hostActivity.set(null)
                    }
                }
            }
        }
    }

    private fun <T> onMainThread(block: () -> T): T {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            return block()
        }

        val result = AtomicReference<T>()
        getInstrumentation().runOnMainSync { result.set(block()) }
        return result.get()
    }

    companion object {
        const val DEFAULT_NATIVE_LIBRARY_NAME = "valdi"

        private const val DEFAULT_RENDER_TIMEOUT_MS = 30_000L

        private var runtimeManager: ValdiRuntimeManager? = null

        private fun sharedRuntimeManager(nativeLibraryName: String?): ValdiRuntimeManager {
            runtimeManager?.let { return it }

            nativeLibraryName?.let { System.loadLibrary(it) }

            // targetContext, not the instrumentation context: the .valdimodule assets and the
            // native library belong to the application under test.
            val created =
                SupportValdiRuntimeManager.createWithSupportLibs(getInstrumentation().targetContext)
            runtimeManager = created
            return created
        }
    }
}
