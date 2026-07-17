package com.snap.valdi.views

import android.content.Context
import android.text.InputType
import android.view.Gravity
import androidx.annotation.Keep

@Keep
class ValdiEditTextMultiline(context: Context) : ValdiEditText(context) {

    // Declared before init so its default is set before the init block's allowLineReturns(true) runs;
    // otherwise the initializer would run afterwards and reset it, defeating idempotence.
    private var appliedLineReturns: Boolean? = null

    init {
        allowLineReturns(true)
        closesWhenReturnKeyPressedDefault = false
        closesWhenReturnKeyPressed = false
        gravity = Gravity.CENTER_VERTICAL
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

    fun allowLineReturns(value: Boolean) {
        // Idempotent: a repeated bind with the same value (e.g. returnType='done' applied twice) must
        // not refresh the live buffer again, which is what surfaced the span/selection crashes.
        if (appliedLineReturns == value) {
            return
        }
        appliedLineReturns = value

        // Snapshot the attributed buffer + selection BEFORE mutating inputType. Copying the live
        // buffer after the input-mode change can read an inconsistent span array and crash.
        val snapshot = snapshotAttributedText()

        inputType = if (value) {
            inputType or InputType.TYPE_TEXT_FLAG_MULTI_LINE
        } else {
            inputType and InputType.TYPE_TEXT_FLAG_MULTI_LINE.inv()
        }
        maxLines = Int.MAX_VALUE
        setHorizontallyScrolling(false)
        applyIgnoreNewlines(!value, snapshot)
    }

}
