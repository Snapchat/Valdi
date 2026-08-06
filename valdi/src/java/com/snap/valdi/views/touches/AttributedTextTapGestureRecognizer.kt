package com.snap.valdi.views.touches

import android.view.MotionEvent
import android.widget.TextView
import com.snap.valdi.attributes.impl.richtext.ValdiProcessedText
import com.snap.valdi.attributes.impl.gestures.TapContext

class AttributedTextTapGestureRecognizer(view: TextView):
        AndroidDetectorGestureRecognizer(view, false) {

    var processedText: ValdiProcessedText? = null

    private var tapContext: TapContext? = null

    override fun onSingleTapUp(event: MotionEvent): Boolean {
        if (processTap(event)) {
            updateState(ValdiGestureRecognizerState.BEGAN)
        } else {
            updateState(ValdiGestureRecognizerState.FAILED)
        }
        return true
    }

    override fun shouldBegin(): Boolean {
        return true
    }

    override fun onUpdate(event: MotionEvent) {
        if (state == ValdiGestureRecognizerState.POSSIBLE) {
            gestureDetector.onTouchEvent(event)
        }
    }

    override fun onProcess() {
        tapContext?.onRecognized(this, state, x, y, pointerCount, pointerLocations)
    }

    override fun onReset(event: MotionEvent) {
        super.onReset(event)

        tapContext = null
    }

    private fun processTap(event: MotionEvent): Boolean {
        tapContext = null

        val textView = this.view as TextView
        val processedText = this.processedText ?: return false
        val spannable = processedText.spannable

        val offset = textView.getOffsetForPosition(event.x, event.y)
        if (offset < 0 || offset >= spannable.length) {
            return false
        }

        val onTap = processedText.onTapAtIndex(offset) ?: return false
        tapContext = TapContext(onTap.value, null)

        return true
    }

    override fun requiresFailureOf(other: ValdiGestureRecognizer): Boolean {
        return other is DoubleTapGestureRecognizer
    }

}
