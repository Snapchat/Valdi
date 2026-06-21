package com.snap.valdi.views

import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.callable.ValdiFunction

/**
 * Views that inherit TextView must implement this interface
 * so that the TextViewAttributesBinder can inject the TextViewHelper.
 * Subclasses should also call update() on the helper in the onMeasure() method.
 */
interface ValdiTextHolder {

    var textViewHelper: TextViewHelper?

    var onSelectionChangeFunction: ValdiFunction?

    fun setTextAccessibility(text: CharSequence?)

    fun setValdiSelectable(selectable: Boolean)

    fun setValdiSelection(start: Int, end: Int)

}
