package com.snap.valdi.views

import android.content.Context
import android.text.TextDirectionHeuristic
import android.view.MotionEvent
import android.widget.TextView
import com.snap.valdi.extensions.ViewUtils
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

    // A label became a ValdiTouchTarget so text selection (isTextSelectable) and onTap/gesture
    // recognizers work. But a plain, non-interactive label (the common case) has neither, and its
    // ValdiTextView wrapper is a ViewGroup, whose own captureCandidates() always returns true once
    // its bounds are hit regardless of what its children did -- hitTest() returning false on just
    // this leaf can't stop that. Opting into allowSiblingCaptureBelow does: it keeps sibling-capture
    // iteration going past the wrapper so whatever's behind/around it (e.g. a native tap-to-open
    // handler) still gets reached, for labels with no real touch behavior of their own.
    override val allowSiblingCaptureBelow: Boolean
        get() {
            if (isTextSelectable) {
                return false
            }
            // Check the owner as well as the backing view: gesture recognisers set from JS (onTap and
            // friends) attach to the ValdiTextView wrapper, not to this leaf. Looking only at `this`
            // reports "no touch behaviour" for a label that does have an onTap, which opts it into
            // sibling capture and fires both its own onTap and whatever sits behind it.
            val ownerView = owner
            val hasGestureRecognizers = ViewUtils.getGestureRecognizers(this, false)?.isEmpty() == false ||
                    (ownerView != null && ViewUtils.getGestureRecognizers(ownerView, false)?.isEmpty() == false)
            return !hasGestureRecognizers
        }

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
