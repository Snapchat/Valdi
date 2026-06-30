package com.snap.valdi.views

import android.content.Context
import android.view.Gravity
import androidx.annotation.Keep
import com.snap.valdi.attributes.impl.richtext.TextViewHelper

@Keep
class ValdiEditTextMultiline(context: Context) : ValdiEditText(context) {
    init {
        backingEditTextInput.pressesReturnOnLineBreak = true
        backingEditTextInput.allowsSameViewGestureRecognizersWhenNotEditable = true
        backingEditTextInput.allowLineReturns(true)
        backingEditTextInput.closesWhenReturnKeyPressedDefault = false
        backingEditTextInput.closesWhenReturnKeyPressed = false
        backingEditTextInput.gravity = Gravity.CENTER_VERTICAL
    }

    override fun configureTextViewHelper(helper: TextViewHelper) {
        super.configureTextViewHelper(helper)
        helper.managesNumberOfLines = true
        helper.defaultNumberOfLines = 0
        helper.applyCurrentNumberOfLines()
    }

}
