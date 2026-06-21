package com.snap.valdi.views

import android.content.Context
import android.text.InputType
import android.text.TextUtils
import android.view.Gravity
import androidx.annotation.Keep
import com.snap.valdi.attributes.impl.richtext.TextViewHelper

@Keep
class ValdiEditTextMultiline(context: Context) : ValdiEditText(context) {

    init {
        allowLineReturns(true)
        closesWhenReturnKeyPressedDefault = false
        closesWhenReturnKeyPressed = false
        gravity = Gravity.CENTER_VERTICAL
    }

    override var textViewHelper: TextViewHelper?
        get() = super.textViewHelper
        set(value) {
            super.textViewHelper = value
            value?.managesNumberOfLines = true
            value?.defaultNumberOfLines = 0
            value?.applyCurrentNumberOfLines()
        }

    override fun onTextChanged(text: CharSequence, start: Int, lengthBefore: Int, lengthAfter: Int) {
        super.onTextChanged(text, start, lengthBefore, lengthAfter)

        if (isSettingTextCount == 0) {
            val end = start + lengthAfter - 1
            if (end >= 0 && text.length > end && text.get(end) == '\n') {
                this.onPressedReturn()
            }
        }
    }

    override fun allowsSameViewGestureRecognizers(): Boolean {
        return !isValdiEditable
    }

    fun onNumberOfLinesChanged() {
        if (hasFiniteNumberOfLines()) {
            ellipsize = TextUtils.TruncateAt.END
            if (!isValdiEditable) {
                setTextIsSelectable(false)
                keyListener = null
                isCursorVisible = false
            }
        } else {
            ellipsize = null
            if (!isValdiEditable) {
                setTextIsSelectable(true)
                keyListener = null
                isCursorVisible = false
            }
        }
    }

    private fun hasFiniteNumberOfLines(): Boolean {
        return maxLines != Int.MAX_VALUE
    }

    fun allowLineReturns(value: Boolean) {
        if (value) {
            setValdiInputType(valdiInputType or InputType.TYPE_TEXT_FLAG_MULTI_LINE)
            textViewHelper?.applyCurrentNumberOfLines()
            setHorizontallyScrolling(false)
            setIgnoreNewlines(false)
        } else {
            setValdiInputType(valdiInputType and InputType.TYPE_TEXT_FLAG_MULTI_LINE.inv())
            textViewHelper?.applyCurrentNumberOfLines()
            setHorizontallyScrolling(false)
            setIgnoreNewlines(true)
        }
    }

}
