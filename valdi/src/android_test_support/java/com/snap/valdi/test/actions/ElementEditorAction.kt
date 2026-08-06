package com.snap.valdi.test.actions

import androidx.test.espresso.PerformException
import androidx.test.espresso.UiController
import androidx.test.espresso.action.EditorAction
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.ElementAction
import com.snap.valdi.test.utils.ValdiElementSearcher

class ElementEditorAction : ElementAction {

    override fun getDescription(): String {
        return "input method editor"
    }

    override fun perform(controller: UiController, item: ValdiElementWithRootView) {
        val textView = ValdiElementSearcher.searchViewInstance(item)
        if (textView == null) {
            val exception = ValdiException(
                "Could not find matching TextView instance for element $item"
            )
            throw PerformException.Builder()
                .withActionDescription(getDescription())
                .withViewDescription(item.toString())
                .withCause(exception)
                .build()
        }

        EditorAction().perform(controller, textView)
    }
}
