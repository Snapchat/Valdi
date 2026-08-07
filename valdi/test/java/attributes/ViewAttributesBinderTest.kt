package com.snap.valdi.attributes

import android.content.Context
import android.view.View
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.ViewAttributesBinder
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.animations.ValdiValueAnimation
import com.snap.valdi.drawables.BoxShadowRendererPool
import com.snap.valdi.logger.Logger
import kotlin.math.PI
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class ViewAttributesBinderTest {
    private class CapturingAnimator : ValdiAnimator {
        val animations = mutableListOf<ValdiValueAnimation>()

        override val beginFromCurrentState: Boolean = false

        override fun addValueAnimation(
            key: Any,
            view: View,
            valueAnimation: ValdiValueAnimation?,
            completion: ((success: Boolean) -> Unit)?
        ) {
            valueAnimation?.let { animations.add(it) }
        }
    }

    private object TestLogger : Logger {
        override fun log(level: Int, message: String?) = Unit

        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    private fun makeBinder(context: Context): ViewAttributesBinder {
        return ViewAttributesBinder(context, TestLogger, BoxShadowRendererPool(context, TestLogger))
    }

    @Test
    fun applyTransformAnimatesFromCurrentViewProperties() {
        val context = getApplicationContext<Context>()
        val view = View(context)
        val animator = CapturingAnimator()
        val density = context.resources.displayMetrics.density

        makeBinder(context).applyTransform(
            view,
            arrayOf(10.0, 20.0, 2.0, 3.0, PI),
            animator
        )

        animator.animations.forEach { it.onProgressUpdate(0.5f) }

        assertEquals(5.0f * density, view.translationX, 0.001f)
        assertEquals(10.0f * density, view.translationY, 0.001f)
        assertEquals(1.5f, view.scaleX, 0.001f)
        assertEquals(2.0f, view.scaleY, 0.001f)
        assertEquals(90.0f, view.rotation, 0.001f)
    }
}
