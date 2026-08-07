package com.snap.valdi.test.actions

import androidx.test.espresso.InjectEventSecurityException
import androidx.test.espresso.PerformException
import androidx.test.espresso.UiController
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.ElementAction

class ElementTypeTextAction(
    private val stringToBeTyped: String,
    private val focusAction: ElementAction?
) : ElementAction {

    override fun getDescription(): String {
        return "type text '$stringToBeTyped'"
    }

    override fun perform(controller: UiController, item: ValdiElementWithRootView) {
        // No-op if string is empty.
        if (stringToBeTyped.isEmpty()) {
            return
        }

        if (focusAction != null) {
            focusAction.perform(controller, item)
            controller.loopMainThreadUntilIdle()
        }

        try {
            if (!controller.injectString(stringToBeTyped)) {
                throwTypingFailed(RuntimeException("Failed to type text: $stringToBeTyped"), item)
            }
        } catch (e: InjectEventSecurityException) {
            throwTypingFailed(e, item)
        }
    }

    private fun throwTypingFailed(cause: Exception, item: ValdiElementWithRootView) {
        throw PerformException.Builder()
            .withActionDescription(getDescription())
            .withViewDescription(item.toString())
            .withCause(cause)
            .build()
    }
}
