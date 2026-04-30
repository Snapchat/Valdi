package com.snap.valdi.extensions

import android.content.Context
import android.content.pm.ApplicationInfo
import android.view.View
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.views.ValdiView

import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertEquals

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],manifest=Config.NONE)
internal class ViewUtilsTest {

    @Before
    fun setUp() {
        // Robolectric doesn't load the module manifest, so RTL support flags must be set manually.
        // View.hasRtlSupport() checks both targetSdkVersion >= 17 and FLAG_SUPPORTS_RTL.
        val appInfo = getApplicationContext<Context>().applicationInfo
        appInfo.targetSdkVersion = 28
        appInfo.flags = appInfo.flags or ApplicationInfo.FLAG_SUPPORTS_RTL
    }

    @Test
    fun testViewUtilsSetIsRightToLeft() {
        val context = getApplicationContext<Context>()
        val view = ValdiView(context)
        ViewUtils.setIsRightToLeft(view, true)
        // Root views do not have their layout direction changed by setIsRightToLeft, so we expect it to be LTR
        assertEquals(View.LAYOUT_DIRECTION_LTR, view.layoutDirection)
        assertEquals(true, ViewUtils.isRightToLeft(view))
    }

    // test that a android.widget.TextView's layout direction is changed to RTL when setIsRightToLeft is called with true
    @Test
    fun testViewUtilsSetIsRightToLeftTextView() {
        val context = getApplicationContext<Context>()
        val textView = androidx.appcompat.widget.AppCompatTextView(context)
        ViewUtils.setIsRightToLeft(textView, true)
        assertEquals(true, ViewUtils.isRightToLeft(textView))
        assertEquals(View.LAYOUT_DIRECTION_RTL, textView.layoutDirection)
    }
}