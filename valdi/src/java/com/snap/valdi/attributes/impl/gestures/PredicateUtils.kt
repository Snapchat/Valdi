package com.snap.valdi.attributes.impl.gestures

import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.callable.performSync
import com.snap.valdi.callable.performSyncWithTimeout
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.views.touches.ValdiGesturePointer
import com.snap.valdi.views.touches.ValdiGestureRecognizer
import com.snap.valdi.views.touches.ValdiGestureRecognizerState

object PredicateUtils {

    const val GESTURE_PREDICATE_TIMEOUT_MS = 250L

    inline fun shouldBegin(predicate: ValdiFunction?, gesture: ValdiGestureRecognizer, x: Int, y: Int, pointerCount: Int, pointerLocations: List<ValdiGesturePointer>, additionalParamsCount: Int, timeoutMs: Long? = null, crossinline prepareAdditionalParams: (ValdiMarshaller, Int) -> Unit): Boolean {
        predicate ?: return true

        return ValdiMarshaller.use {
            val objectIndex = GestureAttributesUtils.pushGestureParams(
                it,
                gesture.view,
                ValdiGestureRecognizerState.POSSIBLE,
                x,
                y,
                pointerCount,
                pointerLocations,
                additionalParamsCount
            )
            prepareAdditionalParams(it, objectIndex)

            val disableDeadline = ViewUtils.findValdiContext(gesture.view)?.runtimeOrNull?.manager?.tweaks?.disableHitTestSyncDeadline == true
            val hasValue = if (timeoutMs != null && !disableDeadline) {
                predicate.performSyncWithTimeout(it, false, timeoutMs)
            } else {
                predicate.performSync(it, false)
            }
            if (hasValue) {
                it.getBoolean(-1)
            } else {
                false
            }
        }
    }

}

