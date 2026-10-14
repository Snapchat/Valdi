package com.snap.valdi.views

import android.content.Context
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.ScrollViewAttributesBinder
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.views.touches.DisallowInterceptTouchEventMode
import com.snap.valdi.views.touches.ScrollViewDragGestureRecognizer
import com.snap.valdi.views.touches.TapGestureRecognizer
import com.snap.valdi.views.touches.TapGestureRecognizerListener
import com.snap.valdi.views.touches.TouchDispatcher
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiScrollViewTest {
    @Test
    fun stopScrollingOnTouchDeliversTapDuringRebound() = forEachDispatcher { scenario ->
        scenario.scroll.stopScrollingOnTouch = true
        scenario.startRebound()

        scenario.send(MotionEvent.ACTION_DOWN)

        assertFalse(scenario.recognizer.isAnimatingScroll)
        assertEquals(scenario.scroll.contentOffsetY, scenario.scroll.unclampedContentOffsetY)
        assertEquals(ValdiGestureRecognizerState.POSSIBLE, scenario.recognizer.state)
        scenario.send(MotionEvent.ACTION_UP)

        assertEquals(1, scenario.tapCount)
    }

    @Test
    fun defaultScrollingConsumesTapDuringRebound() = forEachDispatcher { scenario ->
        scenario.startRebound()

        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_UP)

        assertEquals(0, scenario.tapCount)
    }

    @Test
    fun dragAfterStoppingReboundScrollsWithoutTapping() = forEachDispatcher { scenario ->
        scenario.scroll.stopScrollingOnTouch = true
        scenario.startRebound()

        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_MOVE, 40f)
        scenario.send(MotionEvent.ACTION_UP, 40f)

        assertTrue(scenario.scroll.contentOffsetY > 0)
        assertEquals(0, scenario.tapCount)
    }

    @Test
    fun resettingStopScrollingOnTouchRestoresDefaultGestureBehavior() = forEachDispatcher { scenario ->
        val binder = ScrollViewAttributesBinder(CoordinateResolver(1f), object : Logger {
            override fun log(level: Int, message: String?) {}
            override fun log(level: Int, err: Throwable?, message: String?) {}
        })
        binder.applyStopScrollingOnTouch(scenario.scroll, true, null)
        assertTrue(scenario.scroll.stopScrollingOnTouch)
        binder.resetStopScrollingOnTouch(scenario.scroll, null)
        assertFalse(scenario.scroll.stopScrollingOnTouch)
        scenario.startRebound()

        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_UP)

        assertEquals(0, scenario.tapCount)
    }

    private fun forEachDispatcher(test: (ScrollScenario) -> Unit) {
        for (newMultiTouchExperience in listOf(false, true)) {
            test(ScrollScenario(newMultiTouchExperience))
        }
    }

    private class ScrollScenario(newMultiTouchExperience: Boolean) {
        private val context = getApplicationContext<Context>()
        private val root = ValdiRootView(context).apply {
            useNewMultiTouchExperience = newMultiTouchExperience
        }
        val scroll = ValdiScrollView(context)
        val recognizer: ScrollViewDragGestureRecognizer
        private val dispatcher: TouchDispatcher
        private var downTime = 0L
        var tapCount = 0

        init {
            root.addView(scroll)
            root.layout(0, 0, 100, 400)
            scroll.layout(0, 0, 100, 400)
            scroll.onScrollSpecsChanged(0, 0, 100, 1200, false)
            scroll.contentView.layout(0, 0, 100, 1200)
            val button = View(context)
            scroll.addValdiChildView(button, 0)
            button.layout(0, 0, 100, 400)
            ViewUtils.addGestureRecognizer(button, TapGestureRecognizer(button, object : TapGestureRecognizerListener {
                override fun onRecognized(gesture: ValdiGestureRecognizer, state: ValdiGestureRecognizerState, x: Int, y: Int, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>) {
                    if (state == ValdiGestureRecognizerState.ENDED) {
                        tapCount++
                    }
                }

                override fun shouldBegin(gesture: ValdiGestureRecognizer, x: Int, y: Int, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>): Boolean = true
            }))
            val field = ValdiScrollView::class.java.getDeclaredField("dragGestureRecognizer")
            field.isAccessible = true
            recognizer = field.get(scroll) as ScrollViewDragGestureRecognizer
            dispatcher = TouchDispatcher.create(root, DisallowInterceptTouchEventMode.DISALLOW_WHEN_GESTURE_POSSIBLE, null)
        }

        fun startRebound() {
            scroll.onRecognized(recognizer, ValdiGestureRecognizerState.BEGAN, 50, 100, 0, 80, 0f, 0f, 1, emptyList())
            scroll.onRecognized(recognizer, ValdiGestureRecognizerState.ENDED, 50, 180, 0, 80, 0f, 0f, 1, emptyList())
            SystemClock.sleep(290)
            scroll.run()
            assertTrue(recognizer.isAnimatingScroll)
            assertTrue(scroll.unclampedContentOffsetY < 0)
        }

        fun send(action: Int, y: Float = 100f) {
            SystemClock.sleep(16)
            val now = SystemClock.uptimeMillis()
            if (action == MotionEvent.ACTION_DOWN) {
                downTime = now
            }
            val event = MotionEvent.obtain(downTime, now, action, 50f, y, 0)
            dispatcher.dispatchTouch(event)
            event.recycle()
        }
    }
}
