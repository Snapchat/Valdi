package com.snap.valdi.views

import android.content.Context
import android.text.TextDirectionHeuristic
import android.view.MotionEvent
import android.widget.TextView
import com.snap.valdi.utils.trace

class ValdiTextView(context: Context) :
    ValdiTextViewBase(context, ValdiLabelBackingTextView(context)) {

    init {
        TextViewUtils.configure(backingTextView)
    }

    var text: CharSequence?
        get() = backingTextView.text
        set(value) {
            backingTextView.text = value
        }

    val isTextSelectable: Boolean
        get() = backingTextView.isTextSelectable

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        trace({"ValdiTextView.onMeasure"}) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec)
        }
    }

    override fun resolveBackingHeightMeasureSpec(heightMeasureSpec: Int): Int {
        return TextViewUtils.resolveHeightMeasureSpec(backingTextView, heightMeasureSpec)
    }
}

class ValdiLabelBackingTextView(context: Context) : TextView(context), ValdiTouchTarget {
    private val owner: ValdiTextViewBase?
        get() = parent as? ValdiTextViewBase

    override fun getTextDirectionHeuristic(): TextDirectionHeuristic {
        return TextViewUtils.resolveTextDirectionHeuristic(super.getTextDirectionHeuristic())
    }

    override fun onSelectionChanged(selStart: Int, selEnd: Int) {
        super.onSelectionChanged(selStart, selEnd)
        val owner = owner ?: return
        ValdiTextSelection.notifySelectionChanged(owner, selStart, selEnd)
        ValdiTextSelection.callSelectionChangeCallback(owner.onSelectionChangeFunction, text, selStart, selEnd)
    }

    override fun allowsSameViewGestureRecognizers(): Boolean = true

    override fun processTouchEvent(event: MotionEvent): ValdiTouchEventResult {
        if (!isTextSelectable) {
            return ValdiTouchEventResult.IgnoreEvent
        }

        return if (dispatchTouchEvent(event)) {
            ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        } else {
            ValdiTouchEventResult.IgnoreEvent
        }
    }
}
