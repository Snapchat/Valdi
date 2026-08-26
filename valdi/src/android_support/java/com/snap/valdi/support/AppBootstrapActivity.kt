package com.snap.valdi.support

import android.content.Intent
import android.content.pm.ApplicationInfo
import android.os.Bundle
import android.util.Log
import android.view.View
import androidx.appcompat.app.AppCompatActivity

import com.snap.valdi.IValdiRuntime
import com.snap.valdi.views.ValdiRootView
import com.snap.valdi.support.SupportValdiRuntimeManager
import com.snap.valdi.support.NavigationView
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.ValdiRuntime
import com.snap.valdi.utils.Disposable
import com.snap.valdi.support.DefaultNavigator
import com.snapchat.client.valdi.NativeBridge

/** Intent extra used to select the Valdi debugger port for a debuggable application. */
const val VALDI_DEBUGGER_PORT_INTENT_EXTRA = "com.snap.valdi.DEBUGGER_PORT"

private const val VALDI_LOG_TAG = "Valdi"

/**
  This class implements an Android activity where the root view
  is rendered using a Valdi component. The activity will load libvaldi.so
  when its created, setup a Valdi RuntimeManager and Runtime, and
  "createRootView" so that subclasses can provide the actual root view to
  use.
 */
abstract class AppBootstrapActivity: AppCompatActivity() {

    /**
      Will be called in onCreate() to resolve the root view to use for the
      activity. Subclasses should override this method.
     */
    abstract fun createRootView(bootstrapper: AppBootstrapper): ValdiRootView

    private var rootView: View? = null
    private var navigationView: NavigationView? = null

    lateinit var runtime: ValdiRuntime
        private set
    lateinit var runtimeManager: ValdiRuntimeManager
        private set

    private var valdiRootView: ValdiRootView? = null

    fun getContentWidth(): Int {
        return rootView?.width ?: 0
    }

    fun getContentHeight(): Int {
        return rootView?.height ?: 0
    }

    open fun getNativeLibName(): String {
        return "valdi";
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val debuggerPort = requestedValdiDebuggerPort(
            intent,
            applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE != 0,
        )
        loadNativeLibrary()
        if (debuggerPort != null) {
            // This process-wide override intentionally persists for this debug host process. When the intent
            // omits the extra, this block is skipped so externally configured environment state remains unchanged.
            setDebuggerPortEnvironment(debuggerPort)
        }

        bootstrapValdiRuntime()
    }

    protected open fun loadNativeLibrary() {
        System.loadLibrary(getNativeLibName())
    }

    protected open fun setDebuggerPortEnvironment(debuggerPort: Int) {
        NativeBridge.setDebuggerPortEnvironment(debuggerPort)
    }

    protected open fun bootstrapValdiRuntime() {
        createRuntimeManager()
        this.rootView = createAppRootView()
        setContentView(this.rootView)
    }

    private fun createAppRootView(): View {
        val navigationView = NavigationView(this)
        this.navigationView = navigationView
        val bootstrapper = AppBootstrapper(runtimeManager, runtime, DefaultNavigator(navigationView, runtime))
        val rootView = createRootView(bootstrapper)
        this.valdiRootView = rootView
        navigationView.push(rootView, false)

        return navigationView
    }

    private fun createRuntimeManager() {
        val runtimeManager = SupportValdiRuntimeManager.createWithSupportLibs(this.applicationContext)

        val runtime = runtimeManager.mainRuntime

        this.runtime = runtime
        this.runtimeManager = runtimeManager
    }

    override fun onDestroy() {
        super.onDestroy()
        this.runtime.destroy()
        this.runtimeManager.destroy()
    }

    fun push(view: View) {
        navigationView?.push(view, true)
    }

    override fun onBackPressed() {
        val listener = this.valdiRootView?.onBackButtonListener
        if (listener != null && listener.onBackButtonPressed()) {
            return
        }

        val popped = navigationView?.pop(true) {
            (it as? Disposable)?.dispose()
        } ?: false

        if (!popped) {
            // Nothing handled the back press, let the platform close the activity
            super.onBackPressed()
        }
    }
}

internal fun requestedValdiDebuggerPort(intent: Intent?, debuggable: Boolean): Int? {
    if (!debuggable || intent == null || !intent.hasExtra(VALDI_DEBUGGER_PORT_INTENT_EXTRA)) {
        return null
    }

    val port = intent.extras?.get(VALDI_DEBUGGER_PORT_INTENT_EXTRA) as? Int
    if (port == null || port !in 1..65535) {
        Log.w(
            VALDI_LOG_TAG,
            "Ignoring invalid Valdi debugger port from intent extra " +
                "$VALDI_DEBUGGER_PORT_INTENT_EXTRA: <redacted> (expected an integer in 1...65535)",
        )
        return null
    }

    return port
}
