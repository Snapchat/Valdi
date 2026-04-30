package com.snap.valdi.attributes.impl.gestures

import android.view.MotionEvent
import android.view.View
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.callable.performSyncWithTimeout
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import kotlin.math.roundToInt

object HitTestUtils {
    // Timeout for hit test callbacks to prevent ANR.
    // If the callback takes longer than this, we return false (not hit) to avoid blocking the UI thread.
    // This is acceptable because hit tests are used for gesture handling and a delayed response
    // is worse than a conservative "not hit" response.
    private const val HIT_TEST_TIMEOUT_MS = 100L

    fun hitTest(jsHitTest: ValdiFunction, view: View, event: MotionEvent): Boolean {
        jsHitTest ?: return true
        val pointerLocations = ArrayList<ValdiGesturePointer>(event.pointerCount)
        PointerUtils.fillPointerLocations(event, pointerLocations)

        return ValdiMarshaller.use { marshaller ->
            val objectIndex = GestureAttributesUtils.pushGestureParams(
                marshaller,
                view,
                ValdiGestureRecognizerState.POSSIBLE,
                event.x.roundToInt(),
                event.y.roundToInt(),
                event.pointerCount,
                pointerLocations,
                0
            )

            val hasValue = jsHitTest.performSyncWithTimeout(marshaller, false, HIT_TEST_TIMEOUT_MS)
            if (hasValue) {
                marshaller.getBoolean(-1)
            } else {
                // Timeout or error occurred, return false (conservative default for hit test)
                false
            }
        }
    }

}
