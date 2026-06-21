package com.snap.valdi.views

import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.callable.ValdiFunction

/**
 * Views that render Valdi text implement this interface so text attribute binders
 * can install and update their TextViewHelper.
 */
interface ValdiTextHolder {

    var textViewHelper: TextViewHelper?

    var onSelectionChangeFunction: ValdiFunction?

    fun setTextAccessibility(text: CharSequence?)

    fun setValdiSelectable(selectable: Boolean)

    fun setValdiSelection(start: Int, end: Int)

    fun refreshInlineTextAnimation()

}
