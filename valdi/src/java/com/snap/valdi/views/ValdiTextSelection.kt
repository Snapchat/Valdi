package com.snap.valdi.views

import android.text.Selection
import android.text.Spannable
import android.text.SpannableString
import android.view.View
import android.widget.TextView
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.InternedString
import com.snap.valdi.utils.ValdiMarshaller

object ValdiTextSelection {
    const val EXPECTED_SELECTION_DATA_SIZE = 2

    private val selectionProperty = InternedString.create("selection")
    private val textProperty = InternedString.create("text")
    private val selectionStartProperty = InternedString.create("selectionStart")
    private val selectionEndProperty = InternedString.create("selectionEnd")

    fun setSelectionClamped(textView: TextView, start: Int, end: Int) {
        val spannable = ensureSpannableText(textView)
        val length = spannable.length
        val startClamped = start.coerceIn(0, length)
        val endClamped = end.coerceIn(startClamped, length)
        Selection.setSelection(spannable, startClamped, endClamped)
        if (textView.isTextSelectable && startClamped != endClamped) {
            textView.requestFocus()
        }
        textView.postInvalidateOnAnimation()
    }

    fun notifySelectionChanged(view: View, selectionStart: Int, selectionEnd: Int) {
        ViewUtils.notifyAttributeChanged(view, selectionProperty, intArrayOf(selectionStart, selectionEnd))
    }

    fun callSelectionChangeCallback(
        callback: ValdiFunction?,
        text: CharSequence,
        selectionStart: Int,
        selectionEnd: Int
    ) {
        if (callback == null) {
            return
        }

        ValdiMarshaller.use {
            val objectIndex = it.pushMap(3)
            it.putMapPropertyString(textProperty, objectIndex, text.toString())
            it.putMapPropertyDouble(selectionStartProperty, objectIndex, selectionStart.toDouble())
            it.putMapPropertyDouble(selectionEndProperty, objectIndex, selectionEnd.toDouble())
            callback.perform(it)
        }
    }

    private fun ensureSpannableText(textView: TextView): Spannable {
        val currentText = textView.text
        val spannable = currentText as? Spannable
        if (spannable != null) {
            return spannable
        }

        val newText = SpannableString(currentText ?: "")
        textView.setText(newText, TextView.BufferType.SPANNABLE)
        return textView.text as? Spannable ?: newText
    }
}
