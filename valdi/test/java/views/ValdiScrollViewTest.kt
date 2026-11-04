package com.snap.valdi.views

import android.content.Context
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.ValdiTweaks
import com.snap.valdi.attributes.impl.ScrollViewAttributesBinder
import com.snap.valdi.attributes.impl.gestures.GestureAttributes
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.views.touches.DisallowInterceptTouchEventMode
import com.snap.valdi.views.touches.PinchGestureRecognizerListener
import com.snap.valdi.views.touches.PinchGestureRecognizerV2
import com.snap.valdi.views.touches.ScrollViewDragGestureRecognizer
import com.snap.valdi.views.touches.TapGestureRecognizer
import com.snap.valdi.views.touches.TapGestureRecognizerListener
import com.snap.valdi.views.touches.TouchDispatcher
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiScrollViewTest {
    private val secondPointerDown = MotionEvent.ACTION_POINTER_DOWN or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT)
    private val secondPointerUp = MotionEvent.ACTION_POINTER_UP or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT)

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

    @Test
    fun onPinchTurnsOnDragPausingWhenTweakedAndResetRemovesThePinchAndTurnsItOff() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        val attributes = pinchAttributes(scenario, tweaked = true, pinchV2 = true)

        attributes.applyOnPinch(scenario.scroll, noopFunction, null, null)
        assertTrue(scenario.scroll.endsDragOnAdditionalPointer)
        assertNotNull(pinchV2Of(scenario))

        attributes.resetOnPinch(scenario.scroll)
        assertFalse(scenario.scroll.endsDragOnAdditionalPointer)
        assertNull(pinchV2Of(scenario))
    }

    @Test
    fun onPinchLeavesDragPausingOffWhenTweakedButPinchV2IsOff() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        val attributes = pinchAttributes(scenario, tweaked = true, pinchV2 = false)

        attributes.applyOnPinch(scenario.scroll, noopFunction, null, null)

        assertFalse(scenario.scroll.endsDragOnAdditionalPointer)
    }

    @Test
    fun onPinchLeavesDragPausingOffWhenTweakedOnTheMultiTouchDispatcher() {
        val scenario = ScrollScenario(newMultiTouchExperience = true)
        val attributes = pinchAttributes(scenario, tweaked = true, pinchV2 = true)

        attributes.applyOnPinch(scenario.scroll, noopFunction, null, null)

        assertFalse(scenario.scroll.endsDragOnAdditionalPointer)
    }

    @Test
    fun onPinchLeavesDragPausingOffByDefault() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        val attributes = pinchAttributes(scenario, tweaked = false, pinchV2 = true)

        attributes.applyOnPinch(scenario.scroll, noopFunction, null, null)

        assertFalse(scenario.scroll.endsDragOnAdditionalPointer)
    }

    private val noopFunction = object : ValdiFunction {
        override fun perform(marshaller: ValdiMarshaller): Boolean = true
    }

    private fun pinchAttributes(scenario: ScrollScenario, tweaked: Boolean, pinchV2: Boolean): GestureAttributes {
        scenario.root.enablePinchGestureRecognizeV2 = pinchV2
        return GestureAttributes(
            CoordinateResolver(1f),
            { ValdiTweaks(enableScrollPinchWhileDragging = tweaked) },
            { scenario.root }
        )
    }

    private fun pinchV2Of(scenario: ScrollScenario) =
        ViewUtils.getGestureRecognizers(scenario.scroll, createIfNeeded = false)
            ?.getGestureRecognizer(PinchGestureRecognizerV2::class.java)

    @Test
    fun additionalPointerEndsDragWhenEnabled() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_MOVE, 40f)
        val offsetBeforeSecondPointer = scenario.scroll.contentOffsetY
        assertTrue(offsetBeforeSecondPointer > 0)

        scenario.sendSecondPointerDown(40f)
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, 0f)

        assertEquals(offsetBeforeSecondPointer, scenario.scroll.contentOffsetY)
    }

    @Test
    fun additionalPointerBeforeDragPreventsDragWhenEnabled() = forEachDispatcher { scenario ->
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.sendSecondPointerDown(100f)
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, 40f)

        assertEquals(0, scenario.scroll.contentOffsetY)
    }

    @Test
    fun disablingEndsDragOnAdditionalPointerRestoresDraggingOnNextTouch() = forEachDispatcher { scenario ->
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.sendSecondPointerDown(100f)
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, 40f)
        scenario.send(MotionEvent.ACTION_UP)
        scenario.scroll.endsDragOnAdditionalPointer = false

        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_MOVE, 40f)

        assertTrue(scenario.scroll.contentOffsetY > 0)
    }

    @Test
    fun additionalPointerKeepsDraggingByDefault() = forEachDispatcher { scenario ->
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_MOVE, 40f)
        val offsetBeforeSecondPointer = scenario.scroll.contentOffsetY

        scenario.sendSecondPointerDown(40f)
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, 0f)

        assertTrue(scenario.scroll.contentOffsetY > offsetBeforeSecondPointer)
    }

    @Test
    fun dragResumesWhenAdditionalPointerLifts() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_MOVE, 40f)
        scenario.sendSecondPointerDown(40f)
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, 0f)
        val offsetDuringPinch = scenario.scroll.contentOffsetY

        scenario.sendSecondPointerUp(0f)
        assertEquals(offsetDuringPinch, scenario.scroll.contentOffsetY)
        scenario.send(MotionEvent.ACTION_MOVE, -20f)
        scenario.send(MotionEvent.ACTION_MOVE, -60f)

        val resumedDelta = scenario.scroll.contentOffsetY - offsetDuringPinch
        assertTrue(resumedDelta > 0)
        assertTrue(resumedDelta <= 60)
    }

    @Test
    fun dragStartsAfterAdditionalPointerLiftsBeforeAnyDrag() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.sendSecondPointerDown(100f)
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, 40f)
        assertEquals(0, scenario.scroll.contentOffsetY)

        scenario.sendSecondPointerUp(40f)
        scenario.send(MotionEvent.ACTION_MOVE, 0f)
        scenario.send(MotionEvent.ACTION_MOVE, -40f)

        assertTrue(scenario.scroll.contentOffsetY > 0)
    }

    @Test
    fun dragStaysSuppressedWhileTwoPointersRemainDown() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_MOVE, 40f)
        scenario.sendSecondPointerDown(40f)
        val offsetDuringPinch = scenario.scroll.contentOffsetY
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, 0f)
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, -40f)
        scenario.sendTwoPointers(MotionEvent.ACTION_MOVE, -80f)

        assertEquals(offsetDuringPinch, scenario.scroll.contentOffsetY)
    }

    @Test
    fun pinchKeepsGoingWhenOnePointerLiftsByDefault() {
        val scenario = ScrollScenario(newMultiTouchExperience = false, withPinch = true)
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.sendPinchSpread(secondPointerDown, 20f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 60f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 100f)
        assertEquals(1, scenario.pinchBegan)

        scenario.sendPinchSpread(secondPointerUp, 100f)

        assertEquals(0, scenario.pinchEnded)
    }

    @Test
    fun dragOffsetAccumulatedBeforeThePauseIsDiscarded() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.recognizer.bounces = false
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.send(MotionEvent.ACTION_MOVE, 160f)
        assertEquals(0, scenario.scroll.contentOffsetY)

        scenario.sendSecondPointerDown(160f)
        scenario.sendSecondPointerUp(160f)
        scenario.send(MotionEvent.ACTION_MOVE, 120f)
        scenario.send(MotionEvent.ACTION_MOVE, 60f)

        assertTrue(scenario.scroll.contentOffsetY > 80)
    }

    @Test
    fun pausingTheDragDoesNotFlingTheScrollView() {
        val scenario = ScrollScenario(newMultiTouchExperience = false)
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)
        for (y in listOf(80f, 60f, 40f, 20f)) {
            scenario.send(MotionEvent.ACTION_MOVE, y)
        }
        scenario.sendSecondPointerDown(20f)
        val offsetAtPause = scenario.scroll.contentOffsetY
        assertTrue(offsetAtPause > 0)

        SystemClock.sleep(100)
        scenario.scroll.run()

        assertEquals(offsetAtPause, scenario.scroll.contentOffsetY)
    }

    @Test
    fun pinchCanRestartWhenSecondPointerLiftsAndLandsAgain() {
        val scenario = ScrollScenario(newMultiTouchExperience = false, withPinch = true, pinchEndsOnPointerLift = true)
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)

        scenario.sendPinchSpread(secondPointerDown, 20f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 60f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 100f)
        assertEquals(1, scenario.pinchBegan)

        scenario.sendPinchSpread(secondPointerUp, 100f)
        assertEquals(1, scenario.pinchEnded)

        scenario.sendPinchSpread(secondPointerDown, 20f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 60f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 100f)
        assertEquals(2, scenario.pinchBegan)
    }

    @Test
    fun pinchCanRestartAfterTheRemainingFingerDragsPastSlop() {
        val scenario = ScrollScenario(newMultiTouchExperience = false, withPinch = true, pinchEndsOnPointerLift = true)
        scenario.scroll.endsDragOnAdditionalPointer = true
        scenario.send(MotionEvent.ACTION_DOWN)
        scenario.sendPinchSpread(secondPointerDown, 20f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 60f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 100f)
        scenario.sendPinchSpread(secondPointerUp, 100f)
        assertEquals(1, scenario.pinchEnded)

        scenario.send(MotionEvent.ACTION_MOVE, 60f)
        scenario.send(MotionEvent.ACTION_MOVE, 20f)
        assertTrue(scenario.scroll.contentOffsetY > 0)

        scenario.sendPinchSpread(secondPointerDown, 20f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 60f)
        scenario.sendPinchSpread(MotionEvent.ACTION_MOVE, 100f)
        assertEquals(2, scenario.pinchBegan)
    }

    private fun forEachDispatcher(test: (ScrollScenario) -> Unit) {
        for (newMultiTouchExperience in listOf(false, true)) {
            test(ScrollScenario(newMultiTouchExperience))
        }
    }

    private class ScrollScenario(
        newMultiTouchExperience: Boolean,
        withPinch: Boolean = false,
        pinchEndsOnPointerLift: Boolean = false
    ) {
        private val context = getApplicationContext<Context>()
        val root = ValdiRootView(context).apply {
            useNewMultiTouchExperience = newMultiTouchExperience
        }
        val scroll = ValdiScrollView(context)
        val recognizer: ScrollViewDragGestureRecognizer
        private val dispatcher: TouchDispatcher
        private var downTime = 0L
        var tapCount = 0
        var pinchBegan = 0
        var pinchEnded = 0

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
            if (withPinch) {
                ViewUtils.addGestureRecognizer(scroll, PinchGestureRecognizerV2(scroll, object : PinchGestureRecognizerListener {
                    override fun onRecognized(gesture: ValdiGestureRecognizer, state: ValdiGestureRecognizerState, x: Int, y: Int, scale: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>) {
                        when (state) {
                            ValdiGestureRecognizerState.BEGAN -> pinchBegan++
                            ValdiGestureRecognizerState.ENDED -> pinchEnded++
                            else -> {}
                        }
                    }

                    override fun shouldBegin(gesture: ValdiGestureRecognizer, x: Int, y: Int, scale: Float, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>): Boolean = true
                }, endsOnPointerLift = pinchEndsOnPointerLift))
            }
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

        fun sendSecondPointerDown(y: Float) {
            sendTwoPointers(MotionEvent.ACTION_POINTER_DOWN or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), y)
        }

        fun sendPinchSpread(action: Int, spread: Float) {
            sendTwoPointers(action, 100f, 50f + spread)
        }

        fun sendSecondPointerUp(y: Float) {
            sendTwoPointers(MotionEvent.ACTION_POINTER_UP or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), y)
        }

        fun sendTwoPointers(action: Int, y: Float, secondX: Float = 70f) {
            SystemClock.sleep(16)
            val now = SystemClock.uptimeMillis()
            val properties = Array(2) { id -> MotionEvent.PointerProperties().apply { this.id = id } }
            val coords = arrayOf(
                MotionEvent.PointerCoords().apply { x = 50f; this.y = y },
                MotionEvent.PointerCoords().apply { x = secondX; this.y = y },
            )
            val event = MotionEvent.obtain(downTime, now, action, 2, properties, coords, 0, 0, 1f, 1f, 0, 0, 0, 0)
            dispatcher.dispatchTouch(event)
            event.recycle()
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
