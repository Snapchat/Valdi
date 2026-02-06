package com.snap.valdi.views.touches

import android.graphics.Matrix
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import com.snap.valdi.attributes.impl.gestures.HitTestUtils
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.debug
import com.snap.valdi.utils.dispatchOnMainThread
import com.snap.valdi.views.ValdiTouchEventResult
import com.snap.valdi.views.ValdiTouchTarget
import com.snap.valdi.views.ValdiView

internal class TouchDispatcherImpl(
    override val rootView: ViewGroup,
    override var disallowInterceptTouchEventMode: DisallowInterceptTouchEventMode,
    private val logger: Logger?,
    private val debugTouchEvents: Boolean = false,
    override var cancelsTouchTargetsWhenGestureRequestsExclusivity: Boolean = false,
    override var captureAllHitTargets: Boolean = false,
    override var adjustCoordinates: Boolean = false,
) : TouchDispatcher {

    // Touch lifecycle state
    private val candidateGestureRecognizers = mutableListOf<ValdiGestureRecognizer>()
    private val gestureRecognizersToCancel = mutableListOf<ValdiGestureRecognizer>()

    private val candidateViews = mutableListOf<View>()

    private var gestureRecognizersToStart = mutableListOf<ValdiGestureRecognizer>()

    private var lastEvent: MotionEvent? = null
    private var processScheduled = false
    private var disallowingInterceptTouchEvent = false

    /**
     * Performs a hit test of the given MotionEvent with the given View.
     * The MotionEvent's x/y should be in the View's coordinates.
     */
    private fun hitTest(view: View, event: MotionEvent, pointerIndex: Int): Boolean {
        val customizedHitTestResult = (view as? ValdiTouchTarget)?.hitTest(event)
        if (customizedHitTestResult != null) {
            if (debugTouchEvents) {
                logger?.debug("View ${view::class.java.simpleName} has customized hit test result=$customizedHitTestResult")
            }
            return customizedHitTestResult
        }

        ViewUtils.getGestureRecognizers(view, false)?.hitTest?.let { jsHitTest ->

            val jsHitTestResult = HitTestUtils.hitTest(jsHitTest, view, event)
            if (debugTouchEvents) {
                logger?.debug("View ${view::class.java.simpleName} has js hit test result=$jsHitTestResult")
            }
            return jsHitTestResult
        }


        if (!adjustCoordinates && (view.scaleX != 1f || view.scaleY != 1f || view.rotation != 0f)) {
            return hitTestTransformed(view, event, pointerIndex)
        }

        val touchInsets = ViewUtils.getTouchAreaInsets(view)

        val eventX = event.getX(pointerIndex)
        if (eventX < -(touchInsets?.left ?: 0f)) {
            return false
        }

        val eventY = event.getY(pointerIndex)
        if (eventY < -(touchInsets?.top ?: 0f)) {
            return false
        }

        if (eventX > (view.width.toFloat() + (touchInsets?.right ?: 0f))) {
            return false
        }

        if (eventY > (view.height.toFloat() + (touchInsets?.bottom ?: 0f))) {
            return false
        }

        return true
    }

    /**
     * A version of hitTest that supports views with transforms (scale + rotation).
     */
    private fun hitTestTransformed(view: View, event: MotionEvent, pointerIndex: Int): Boolean {
        if (view.scaleX == 0.0f || view.scaleY == 0.0f) {
            return false
        }

        val width = view.width.toFloat()
        val height = view.height.toFloat()
        val halfWidth = width / 2
        val halfHeight = height / 2

        val eventX =  event.getX(pointerIndex)
        val eventY =  event.getY(pointerIndex)

        // Translate the points so the origin (0, 0) is in the middle of the view
        val eventPoint = Pair(eventX - halfWidth, eventY - halfHeight)
        val rectPoints = floatArrayOf(
            -halfWidth, -halfHeight,
            halfWidth, -halfHeight,
            -halfWidth, halfHeight,
            halfWidth, halfHeight
        )

        val mat = Matrix()
        mat.postScale(view.scaleX, view.scaleY)
        mat.postRotate(view.rotation)
        mat.mapPoints(rectPoints)

        val a = Pair(rectPoints[0], rectPoints[1])
        val b = Pair(rectPoints[2], rectPoints[3])
        val c = Pair(rectPoints[4], rectPoints[5])
        val d = Pair(rectPoints[6], rectPoints[7])

        // The event point is within the transformed view bounds if the sum of all triangle areas is less than
        // or equal to the total area of the scaled view
        val sum = areaBetweenPoints(a, b, eventPoint) + areaBetweenPoints(b, c, eventPoint) +
            areaBetweenPoints(c, d, eventPoint) + areaBetweenPoints(d, a, eventPoint)
        return sum <= width * Math.abs(view.scaleX) * height * Math.abs(view.scaleY)
    }

    private fun areaBetweenPoints(a: Pair<Float, Float>, b: Pair<Float, Float>, c: Pair<Float, Float>): Float {
        return Math.abs(((a.first * (b.second - c.second)) +
            (b.first * (c.second - a.second)) +
            (c.first * (a.second - b.second))) / 2)
    }

    private fun captureCandidates(view: View, event: MotionEvent, pointerIndex: Int): Boolean {
        if (!view.isEnabled || view.alpha == 0f || view.visibility == View.INVISIBLE || !hitTest(view, event, pointerIndex)) {
            if (debugTouchEvents) {
                logger?.debug("View ${view.logTag()} did NOT hit ${view.width}/${view.height} with touch ${event.getX(pointerIndex)}/${event.getY(pointerIndex)}")
            }
            return false
        }
        if (debugTouchEvents) {
            logger?.debug( "View ${view.logTag()} DID hit ${view.width}/${view.height} with touch ${event.getX(pointerIndex)}/${event.getY((pointerIndex))}")
        }

        // explicitly exclude custom native ValdiTouchTarget's children views from being scanned for valdi touch targets
        if (view is ValdiTouchTarget) {
            if (debugTouchEvents) {
                logger?.debug("Valdi touch target ${view::class.java.simpleName}-${System.identityHashCode(view)} wants to handle touch event")
            }
            candidateViews.add(view)
            return true
        }

        // If the view is a view group, then we need to check if this view can intercept the touch event
        if (view is ViewGroup) {
            // Dispatch touches to children
            for (i in view.childCount - 1 downTo 0) {
                val child = view.getChildAt(i)
                child ?: continue

                // TODO - actually fix hitTest for gestures for all events
                val didHit = adjustEventCoordinatesToView(view, child, event)
                { _, adjustedEvent ->
                    captureCandidates(child, adjustedEvent, pointerIndex)
                }

                if (didHit && !captureAllHitTargets) {
                    break
                }
            }
        }

        val gestureRecognizer = ViewUtils.getGestureRecognizers(view, false) ?: return true

        gestureRecognizer.gestureRecognizers.forEach {
            // TODO(2951) for now - we choose to only add touch gesture recognizers for subsequent pointer downs
            if (pointerIndex == 0 || it is TouchGestureRecognizer) {
                if (!candidateGestureRecognizers.contains(it)) {
                    if (debugTouchEvents) {
                        logger?.debug("Adding candidate gesture recognizer $it to TouchDispatcher-${System.identityHashCode(this)} for event ${event.actionMasked}")
                    }
                    candidateGestureRecognizers.add(it)
                }
            }
        }

        return true
    }

    private inline fun <T> adjustEventCoordinatesToView(
        parentView: View,
        view: View,
        event: MotionEvent,
        crossinline action: (success: Boolean, adjustedEvent: MotionEvent) -> T
    ): T {
        // this adjusts x/y to fit the view bounds regardless of pointer, in an even way
        val originalX = event.x
        val originalY = event.y

        var offsetX = 0f
        var offsetY = 0f
        var current = view as? View
        while (current != null && current !== parentView) {
            offsetX -= current.x
            offsetY -= current.y

            current = current.parent as? View

            if (current != null) {
                val scrollX = current.scrollX
                if (scrollX != 0) {
                    offsetX += scrollX.toFloat()
                }
                val scrollY = current.scrollY
                if (scrollY != 0) {
                    offsetY += scrollY.toFloat()
                }
            }
        }

        if (current == null) {
            // View is not a subview of the given parentView.
            return action(false, event)
        }

        val newEvent = if (adjustCoordinates) {
            ViewUtils.obtainAdjustedMotionEvent(view, event)
        } else {
            event.setLocation(originalX + offsetX, originalY + offsetY)
            event
        }

        try {
            return action(true, newEvent)
        } finally {
            if (adjustCoordinates) {
                newEvent.recycle()
            } else {
                event.setLocation(originalX, originalY)
            }
        }
    }

    private fun canRecognizeSimultaneously(leftGestureRecognizer: ValdiGestureRecognizer, rightGestureRecognizer: ValdiGestureRecognizer): Boolean {
        if (leftGestureRecognizer.canRecognizeSimultaneously(rightGestureRecognizer)) {
            return true
        }
        if (rightGestureRecognizer.canRecognizeSimultaneously(leftGestureRecognizer)) {
            return true
        }
        return false
    }

    private fun isDeferred(gestureRecognizer: ValdiGestureRecognizer): Boolean {
        return gestureRecognizersToStart.contains(gestureRecognizer)
    }

    private fun processGestureRecognizers() {
        val event = lastEvent ?: return

        // Step 3, we update all the active gesture recognizers. They will update their states.
        var index = 0
        while (index < candidateGestureRecognizers.size) {
            val gestureRecognizer = candidateGestureRecognizers[index]

            if (!isDeferred(gestureRecognizer)) {
                if (debugTouchEvents) {
                    logger?.debug("Considering $gestureRecognizer for touch handling")
                }

                val before = gestureRecognizer.state
                adjustEventCoordinatesToView(rootView, gestureRecognizer.view, event)
                { processed, adjustedEvent ->
                    if (processed) {
                        if (debugTouchEvents) {
                            logger?.debug("update ${gestureRecognizer::class.simpleName} with event ${event.x} adjusted ${adjustedEvent.x}")
                        }

                        gestureRecognizer.update(adjustedEvent, adjustCoordinates)
                    } else {
                        if (debugTouchEvents) {
                            logger?.debug("Canceling $gestureRecognizer since its not part of the same hierarchy anymore.")
                        }

                        gestureRecognizer.cancel(event)
                    }
                }

                if (debugTouchEvents && before != gestureRecognizer.state) {
                    logger?.debug("$gestureRecognizer transitioned to state ${gestureRecognizer.state}")
                }

                if (gestureRecognizer.state == ValdiGestureRecognizerState.FAILED) {
                    if (debugTouchEvents) {
                        logger?.debug("Failed ${gestureRecognizer::class.java.simpleName}-${System.identityHashCode(gestureRecognizer)} of TouchDispatcher-${System.identityHashCode(this)} with state ${gestureRecognizer.state} for touch handling")
                    }

                    candidateGestureRecognizers.removeAt(index)
                    gestureRecognizer.cancel(event)
                    index--
                } else if (gestureRecognizer.state == ValdiGestureRecognizerState.BEGAN) {
                    if (debugTouchEvents) {
                        logger?.debug("Started ${gestureRecognizer::class.java.simpleName}-${System.identityHashCode(gestureRecognizer)} of TouchDispatcher-${System.identityHashCode(this)} with state ${gestureRecognizer.state} for touch handling")
                    }
                    gestureRecognizersToStart.add(gestureRecognizer)
                }
            }

            index++
        }

        // Step 4, we go over all the gesture recognizers which want to start. We resolve the conflicts.
        // For each gesture which wants to start, we will ask any other active gestures or gestures which want to
        // start if they agree that this gesture should be allowed to start. If any one the gesture says no, the
        // gesture will not start. As such we go from the back, as the gestures which are at the front are deepest
        // in the hierarchy, they should be prioritized and canceled only as a last resort.
        index = gestureRecognizersToStart.size
        while (index > 0) {
            index--

            val gestureRecognizerToStart = gestureRecognizersToStart[index]
            var shouldStart = true
            var shouldDefer = false

            // Cancel any conflicting gestures that either can't be simultaneously recognized or require failure
            var downIndex = index
            while (downIndex > 0) {
                downIndex--
                val conflictingGestureRecognizerToStart = gestureRecognizersToStart[downIndex]
                if (conflictingGestureRecognizerToStart.requiresFailureOf(gestureRecognizerToStart)) {
                    // The other conflicting gesture required failure of this gesture but this gesture succeeded,
                    // so we cancel the other conflicting gesture.
                    if (debugTouchEvents)
                        logger?.debug("Canceling $gestureRecognizerToStart since it conflicted with another gesture $gestureRecognizerToStart")
                    conflictingGestureRecognizerToStart.cancel(event)
                    gestureRecognizersToStart.removeAt(downIndex)
                    if (downIndex < index) {
                        index--
                    }
                    continue
                }
                if (!canRecognizeSimultaneously(
                        gestureRecognizerToStart,
                        conflictingGestureRecognizerToStart
                    )
                ) {
                    // The other conflicting gesture cannot succeed at the same time as this one
                    // so we cancel this gesture as the other gesture will take priority
                    shouldStart = false
                    break
                }
            }

            // If we're not conflicting with anything
            if (shouldStart) {
                // Also look up in the active/possible gesture recognizers
                for (candidateGestureRecognizer in candidateGestureRecognizers) {
                    if (candidateGestureRecognizer === gestureRecognizerToStart) {
                        continue
                    }
                    // Fail if another incompatible gesture started before us
                    if (candidateGestureRecognizer.state == ValdiGestureRecognizerState.CHANGED || candidateGestureRecognizer.state == ValdiGestureRecognizerState.ENDED) {
                        if (!canRecognizeSimultaneously(
                                gestureRecognizerToStart,
                                candidateGestureRecognizer
                            )
                        ) {
                            shouldStart = false
                            break
                        }
                    }
                    // Defer current gesture if we're required to wait until failure of another gesture
                    else if (candidateGestureRecognizer.state == ValdiGestureRecognizerState.POSSIBLE) {
                        if (gestureRecognizerToStart.requiresFailureOf(candidateGestureRecognizer)) {
                            shouldDefer = true
                            break
                        }
                    }
                }
            }

            // If we need to defer, we just keep all elements in their current queues without any change
            if (shouldDefer) {
                if (debugTouchEvents) {
                    logger?.debug("Deferring start of ${gestureRecognizerToStart::class.java.simpleName}")
                }
            }
            // If we already took the decision on the current gesture, we can remove it from the to start gestures
            else {
                // If we failed to take priority, we just cancel the current gesture and remove it from the candidates
                if (!shouldStart) {
                    if (debugTouchEvents) {
                        logger?.debug("Canceling $gestureRecognizerToStart since it conflicted with another gesture")
                    }
                    gestureRecognizerToStart.cancel(event)
                    candidateGestureRecognizers.remove(gestureRecognizerToStart)
                }
                // If we succeeded and the gesture will now start
                else {
                    // If the current gesture wants to cancel other gestures inside of it
                    if (gestureRecognizerToStart.shouldCancelOtherGesturesOnStart()) {
                        // Look for any other candidate gesture that we may have priority over
                        var childIndex = candidateGestureRecognizers.indexOf(gestureRecognizerToStart)
                        while (childIndex > 0) {
                            childIndex--
                            // Cancel the other conflicting gesture
                            val getureRecognizerToCancel = candidateGestureRecognizers[childIndex]
                            getureRecognizerToCancel.cancel(event)
                            candidateGestureRecognizers.removeAt(childIndex)
                            // If the conflicting gesture is also about to start, remove it from the start list
                            val canceledRecognizerToStartIndex = gestureRecognizersToStart.indexOf(getureRecognizerToCancel)
                            if (canceledRecognizerToStartIndex >= 0) {
                                gestureRecognizersToStart.removeAt(canceledRecognizerToStartIndex)
                                if (canceledRecognizerToStartIndex < index) {
                                    index--
                                }
                            }
                        }

                        if (cancelsTouchTargetsWhenGestureRequestsExclusivity) {
                            candidateViews.clear()
                        }
                    }
                }
                // Done
                gestureRecognizersToStart.remove(gestureRecognizerToStart)
            }
        }

        // Step 5, we process all active gesture recognizers,
        index = 0
        while (index < candidateGestureRecognizers.size) {
            val gestureRecognizer = candidateGestureRecognizers[index]

            if (!isDeferred(gestureRecognizer) && gestureRecognizer.isActive) {
                gestureRecognizer.process()

                if (gestureRecognizer.state == ValdiGestureRecognizerState.ENDED) {
                    if (debugTouchEvents) {
                        logger?.debug("${gestureRecognizer::class.java.simpleName} ended")
                    }

                    gestureRecognizer.cancel(event)
                    candidateGestureRecognizers.removeAt(index)
                    index--
                }
            }

            index++
        }

    }

    private fun hasActiveTouchEvents(): Boolean {
        return candidateGestureRecognizers.isNotEmpty() || candidateViews.isNotEmpty()
    }

    private fun eventIsUp(event: MotionEvent): Boolean {
        return when(event.actionMasked) {
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> true
            else -> false
        }
    }

    override fun gestureRecognizerWantsDeferredUpdate(gestureRecognizer: ValdiGestureRecognizer): Boolean {
        val willReceiveUpdate = candidateGestureRecognizers.contains(gestureRecognizer)

        if (!processScheduled) {
            processScheduled = true
            dispatchOnMainThread {
                processScheduled = false
                processGestureRecognizers()

                lastEvent?.let {
                    if (eventIsUp(it)) {
                        resetState()
                    }
                }

                updateDisallowInterceptTouchEvent()
            }
        }

        return willReceiveUpdate
    }

    override fun isEmpty(): Boolean {
        return candidateViews.isEmpty() && candidateGestureRecognizers.isEmpty() && gestureRecognizersToStart.isEmpty()
    }

    override fun dispatchTouch(event: MotionEvent): Boolean {
        lastEvent?.recycle()
        lastEvent = MotionEvent.obtain(event)

        val isDown = event.actionMasked == MotionEvent.ACTION_DOWN
        val isPointerDown = event.actionMasked == MotionEvent.ACTION_POINTER_DOWN
        val isUp = eventIsUp(event)
        val isPointerUp = event.actionMasked == MotionEvent.ACTION_POINTER_UP

        if (debugTouchEvents && isDown) {
            logger?.debug("Valdi root view ${System.identityHashCode(this.rootView)} TouchDispatcher-${System.identityHashCode(this)} received touch down event")
        }

        if (debugTouchEvents && isPointerDown) {
            logger?.debug("Valdi root view received pointer down event")
        }

        if (debugTouchEvents && isUp) {
            logger?.debug("Valdi root view ${System.identityHashCode(this.rootView)} TouchDispatcher-${System.identityHashCode(this)} received touch up event")
        }

        if (debugTouchEvents && isPointerUp) {
            logger?.debug("Valdi root view received pointer up event")
        }

        try {
            if (isDown) {
                resetState()
            }

            if (isPointerDown || isDown) {
                // Step 1, we capture all the views and their gesture recognizers which are within the event's target.
                // We only do this on touch down, or for new pointers, we capture all their potential gesture recognizers as well
                captureCandidates(rootView, event, event.actionIndex)
            }

            // Step 2, we go over all the touch targets which want to handle touch,
            // if one of them consumed the touch event, we skip the gesture recognizers
            var candidateViewIndex = 0
            while (candidateViewIndex < candidateViews.size) {
                val candidateView = candidateViews.getOrNull(candidateViewIndex) ?: break

                val touchTarget = candidateView as? ValdiTouchTarget
                if (touchTarget == null) {
                    candidateViewIndex++
                    continue
                }

                if (debugTouchEvents) {
                    logger?.debug("Considering ${touchTarget::class.java.simpleName}-${System.identityHashCode(touchTarget)} for touch handling")
                }

                val touchTargetConsumedEvent =
                    adjustEventCoordinatesToView(rootView, candidateView, event)
                    { processed, adjustedEvent ->
                        if (!processed) {
                            return@adjustEventCoordinatesToView false
                        }
                        if (debugTouchEvents) {
                            logger?.debug(
                                "Trying to consume event with the view: ${touchTarget::class.java.simpleName}-${
                                    System.identityHashCode(
                                        touchTarget
                                    )
                                }... TouchDispatcher-${System.identityHashCode(this)} "
                            )
                        }
                        val touchEventResult = touchTarget.processTouchEvent(adjustedEvent)
                        if (debugTouchEvents) {
                            logger?.debug(
                                "${touchTarget::class.java.simpleName}-${
                                    System.identityHashCode(
                                        touchTarget
                                    )
                                } TouchDispatcher-${System.identityHashCode(this)}  processTouchEvent result: $touchEventResult"
                            )
                        }
                        if (touchEventResult === ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures) {
                            return@adjustEventCoordinatesToView true
                        }
                        return@adjustEventCoordinatesToView false
                    }

                if (touchTargetConsumedEvent) {
                    if (debugTouchEvents) {
                        logger?.debug("View ${touchTarget::class.java.simpleName}-${System.identityHashCode(touchTarget)} TouchDispatcher-${System.identityHashCode(this)} consumed touch event, canceling gesture recognizers")
                    }
                    removeAllGestureRecognizers()
                    return true
                }
                candidateViewIndex++
            }

            processGestureRecognizers()

            return hasActiveTouchEvents()
        } finally {
            if (isUp) {
                resetState()
            }
            updateDisallowInterceptTouchEvent()
        }
    }

    private fun shouldDisallowInterceptTouchEvent(): Boolean {
        for (gesture in candidateGestureRecognizers) {
            if (!gesture.shouldPreventInterceptTouchEvent()) {
                continue
            }

            when (disallowInterceptTouchEventMode) {
                DisallowInterceptTouchEventMode.DISALLOW_WHEN_GESTURE_RECOGNIZED -> {
                    if (gesture.isActive && !isDeferred(gesture)) {
                        return true
                    }
                }
                DisallowInterceptTouchEventMode.DISALLOW_WHEN_GESTURE_POSSIBLE -> {
                    return true
                }
            }
        }

        return false
    }

    override fun getCurrentGesturesState(): GesturesState {
        var hasPossibleGesture = false
        for (gesture in candidateGestureRecognizers) {
            if (!gesture.shouldPreventInterceptTouchEvent()) {
                continue
            }

            hasPossibleGesture = true

            if (gesture.isActive && !isDeferred(gesture)) {
                return GesturesState.HAS_ACTIVE_GESTURES
            }
        }

        if (hasPossibleGesture || candidateViews.isNotEmpty()) {
            return GesturesState.HAS_POSSIBLE_GESTURES
        }

        return GesturesState.INACTIVE
    }

    private fun updateDisallowInterceptTouchEvent() {
        if (shouldDisallowInterceptTouchEvent()) {
            if (!disallowingInterceptTouchEvent) {
                disallowingInterceptTouchEvent = true
                if (debugTouchEvents) {
                    logger?.debug("Disallowing intercept touch event")
                }
                rootView.requestDisallowInterceptTouchEvent(true)
            }
        } else {
            if (disallowingInterceptTouchEvent) {
                disallowingInterceptTouchEvent = false
                if (debugTouchEvents) {
                    logger?.debug("Allowing intercept touch event")
                }
                rootView.requestDisallowInterceptTouchEvent(false)
            }
        }
    }

    private fun resetState() {
        //NOTE(rjaber): This function needs some special care. When cancel is called in practice, it
        //              can trigger ACTION_CANCEL if the gesture recognizer is connected to a
        //              dismissal. This means some gestures can be removed while we're iterating
        //              over the function, and the the last event isn't super reliable.
        candidateViews.clear()
        val storedLastEvent = lastEvent

        var index = candidateGestureRecognizers.size
        while (index > 0) {
            index--

            if (!isDeferred(candidateGestureRecognizers[index])) {
                gestureRecognizersToCancel.add(candidateGestureRecognizers[index])
                candidateGestureRecognizers.removeAt(index)
            }
        }

        if (storedLastEvent != null) {
            gestureRecognizersToCancel.forEach {
                it.cancel(storedLastEvent)
            }
        }
        gestureRecognizersToCancel.clear()
    }

    private fun removeAllGestureRecognizers() {
        lastEvent?.let { event ->
            candidateGestureRecognizers.forEach {
                it.cancel(event)
                if (debugTouchEvents) {
                    logger?.debug("Candidate gesture recognizer ${it::class.java.simpleName}-${System.identityHashCode(it)} removed from TouchDispatcher-${System.identityHashCode(this)}")
                }
            }
        }
        candidateGestureRecognizers.clear()
    }

    private fun View.logTag(): String {
        val accessibilityId = if(this is ValdiView)valdiViewNode?.getAttribute("accessibilityId") else null

        return accessibilityId?.toString() ?: "${this::class.java.simpleName}-${System.identityHashCode(this)}"
    }
}
