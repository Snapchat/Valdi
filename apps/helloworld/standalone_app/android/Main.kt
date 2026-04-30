package com.snap.helloworld

import android.app.Activity
import android.os.Bundle
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.support.SupportValdiRuntimeManager
import com.snap.valdi.modules.hello_world.App

/**
 * Standalone HelloWorld app that uses the exported AAR.
 * 
 * For full integration documentation, see:
 * ../../../../docs/docs/workflow-external-build-system.md#gradle
 */
class StartActivity : Activity() {
    var runtimeManager: ValdiRuntimeManager? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // This needs to execute at least once
        // The name of the library will match the name of
        // your valdi_exported_library() target.
        System.loadLibrary("hello_world_export")

        // For best performance, your app should maintain a singleton
        // of the Valdi runtime manager.
        val runtimeManager = SupportValdiRuntimeManager.createWithSupportLibs(this.applicationContext)
        this.runtimeManager = runtimeManager

        setContentView(App.create(runtimeManager.mainRuntime))
    }
}