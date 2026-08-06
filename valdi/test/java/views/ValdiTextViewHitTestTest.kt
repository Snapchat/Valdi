package com.snap.valdi.views

import android.content.Context
import android.view.MotionEvent
import android.view.View
import android.widget.FrameLayout
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.views.touches.DisallowInterceptTouchEventMode
import com.snap.valdi.views.touches.TouchDispatcher
import com.snap.valdi.views.touches.ValdiGestureRecognizer
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * Regression coverage for the touch-dispatch shadowing bug introduced when [ValdiTextView]
 * was converted from extending [android.widget.TextView] directly to wrapping a backing
 * [ValdiLabelBackingTextView] that implements [ValdiTouchTarget]. Because
 * [com.snap.valdi.views.touches.TouchDispatcherImpl.captureCandidates] (and its
 * [com.snap.valdi.views.touches.TouchDispatcherNewExperience] counterpart) treat any
 * [ValdiTouchTarget] hit as an unconditional claim, a plain, non-interactive label now silently
 * shadowed whatever sat behind/around it on screen (e.g. a native tap-to-reopen handler) unless
 * it opts out via [ValdiTouchTarget.hitTest]. The fix makes the backing view report a hit only
 * when it actually has interactive behavior (text selection or a registered gesture recognizer).
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiTextViewHitTestTest {

    /** Stand-in for a native handler sitting behind the label (e.g. tap-to-reopen-editor). */
    private class FakeTouchTargetView(context: Context) : View(context), ValdiTouchTarget {
        var processed = false

        override fun processTouchEvent(event: MotionEvent): ValdiTouchEventResult {
            processed = true
            return ValdiTouchEventResult.IgnoreEvent
        }
    }

    private class FakeGestureRecognizer(view: View) : ValdiGestureRecognizer(view) {
        override fun shouldBegin(): Boolean = false
        override fun onUpdate(event: MotionEvent) = Unit
        override fun onProcess() = Unit
    }

    private fun makeDownEvent(x: Float, y: Float): MotionEvent =
        MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_DOWN, x, y, 0)

    /** [lower] added first (bottom of z-order), [label] added last (top), both full-bleed. */
    private fun buildOverlappingLabelAndTarget(): Triple<FrameLayout, FakeTouchTargetView, ValdiTextView> {
        val context = getApplicationContext<Context>()
        val root = FrameLayout(context)
        val lower = FakeTouchTargetView(context)
        val label = ValdiTextView(context)
        root.addView(lower)
        root.addView(label)
        root.layout(0, 0, 1000, 1000)
        lower.layout(0, 0, 1000, 1000)
        label.layout(0, 0, 1000, 1000)
        return Triple(root, lower, label)
    }

    private fun dispatch(root: FrameLayout, x: Float = 500f, y: Float = 500f) {
        val dispatcher = TouchDispatcher.create(
            root,
            DisallowInterceptTouchEventMode.DISALLOW_WHEN_GESTURE_POSSIBLE,
            null,
        )
        val event = makeDownEvent(x, y)
        dispatcher.dispatchTouch(event)
        event.recycle()
    }

    @Test
    fun `plain non-interactive label does not shadow the handler behind it`() {
        val (root, lower, _) = buildOverlappingLabelAndTarget()

        dispatch(root)

        assertTrue("handler behind the label should still be reachable", lower.processed)
    }

    @Test
    fun `selectable label still shadows the handler behind it`() {
        val (root, lower, label) = buildOverlappingLabelAndTarget()
        label.setValdiSelectable(true)

        dispatch(root)

        assertFalse("a real interactive (selectable) label should keep claiming its touch", lower.processed)
    }

    @Test
    fun `label with a registered gesture recognizer still shadows the handler behind it`() {
        val (root, lower, label) = buildOverlappingLabelAndTarget()
        ViewUtils.addGestureRecognizer(label.backingTextView, FakeGestureRecognizer(label.backingTextView))

        dispatch(root)

        assertFalse("a label with real gesture behavior should keep claiming its touch", lower.processed)
    }
}
