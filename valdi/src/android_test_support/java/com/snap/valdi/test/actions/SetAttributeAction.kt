package com.snap.valdi.test.actions

import androidx.test.espresso.UiController
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.ElementAction

class SetAttributeAction(val attributeName: String, val attributeValue: Any?) : ElementAction {

    override fun getDescription(): String {
        return "set attribute '$attributeName' to '$attributeValue'"
    }

    override fun perform(controller: UiController, element: ValdiElementWithRootView) {
        element.viewNode.setAttribute(attributeName, attributeValue, true)
    }
}
