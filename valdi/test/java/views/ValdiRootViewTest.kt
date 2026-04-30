package com.snap.valdi.views

import android.app.Activity
import android.content.Context
import android.os.Looper
import android.view.View
import android.view.ViewTreeObserver
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiRootViewTest {

    @Test
    fun clearOnNextDrawPreDrawListener_removesTransferredListener_fromCurrentObserver() {
        val rootView = TestValdiRootView(getApplicationContext())
        val oldObserver = rootView.viewTreeObserver
        var preDrawCallCount = 0
        val preDrawListener = ViewTreeObserver.OnPreDrawListener {
            preDrawCallCount += 1
            true
        }

        oldObserver.addOnPreDrawListener(preDrawListener)
        rootView.setPrivateField("onNextDrawPreDrawListener", preDrawListener)
        rootView.setPrivateField("onNextDrawPreDrawObserver", oldObserver)

        val activity = Robolectric.buildActivity(Activity::class.java).setup().get()
        activity.setContentView(rootView)

        assertFalse(oldObserver.isAlive)

        rootView.invokePrivateMethod("clearOnNextDrawPreDrawListener")

        assertNull(rootView.getPrivateField("onNextDrawPreDrawListener"))
        assertNull(rootView.getPrivateField("onNextDrawPreDrawObserver"))

        rootView.viewTreeObserver.dispatchOnPreDraw()

        assertEquals(0, preDrawCallCount)
    }

    @Test
    fun finalize_posts_onNextDrawCleanup_toMainThread() {
        val rootView = TestValdiRootView(getApplicationContext())
        val attachListener = object : View.OnAttachStateChangeListener {
            override fun onViewAttachedToWindow(v: View) = Unit
            override fun onViewDetachedFromWindow(v: View) = Unit
        }
        val preDrawListener = ViewTreeObserver.OnPreDrawListener { true }

        rootView.setPrivateField("onNextDrawAttachListener", attachListener)
        rootView.setPrivateField("onNextDrawPreDrawListener", preDrawListener)
        rootView.setPrivateField("onNextDrawPreDrawObserver", rootView.viewTreeObserver)

        val finalizeThread = Thread {
            rootView.callFinalizeForTest()
        }
        finalizeThread.start()
        finalizeThread.join()

        assertNotNull(rootView.getPrivateField("onNextDrawAttachListener"))
        assertNotNull(rootView.getPrivateField("onNextDrawPreDrawListener"))
        assertNotNull(rootView.getPrivateField("onNextDrawPreDrawObserver"))

        shadowOf(Looper.getMainLooper()).idle()

        assertNull(rootView.getPrivateField("onNextDrawAttachListener"))
        assertNull(rootView.getPrivateField("onNextDrawPreDrawListener"))
        assertNull(rootView.getPrivateField("onNextDrawPreDrawObserver"))
    }

    private fun Any.setPrivateField(name: String, value: Any?) {
        javaClass.superclass!!.getDeclaredField(name).apply {
            isAccessible = true
            set(this@setPrivateField, value)
        }
    }

    private fun Any.getPrivateField(name: String): Any? {
        return javaClass.superclass!!.getDeclaredField(name).apply {
            isAccessible = true
        }.get(this)
    }

    private fun Any.invokePrivateMethod(name: String) {
        javaClass.superclass!!.getDeclaredMethod(name).apply {
            isAccessible = true
            invoke(this@invokePrivateMethod)
        }
    }

    private class TestValdiRootView(context: Context) : ValdiRootView(context) {
        fun callFinalizeForTest() {
            finalize()
        }
    }
}
