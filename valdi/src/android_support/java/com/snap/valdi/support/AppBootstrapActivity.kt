package com.snap.valdi.support

import android.os.Bundle
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

        System.loadLibrary(getNativeLibName())

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
