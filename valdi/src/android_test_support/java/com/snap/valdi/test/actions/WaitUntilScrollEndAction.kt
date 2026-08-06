package com.snap.valdi.test.actions

import androidx.test.espresso.PerformException
import androidx.test.espresso.UiController
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.ElementAction

class WaitUntilScrollEndAction(val timeoutMs: Long) : ElementAction {

    override fun getDescription(): String {
        return "waiting for scroll end with max time out $timeoutMs milliseconds"
    }

    override fun perform(controller: UiController, item: ValdiElementWithRootView) {
        val currentTime = System.currentTimeMillis()
        while (item.viewNode.isScrollingOrAnimatingScroll()) {
            val elapsed = System.currentTimeMillis() - currentTime
            if (elapsed > timeoutMs) {
                throw PerformException.Builder()
                    .withActionDescription(getDescription())
                    .withViewDescription(item.toString())
                    .withCause(RuntimeException("Timed out waiting for scroll end"))
                    .build()
            }

            controller.loopMainThreadForAtLeast(50L)
        }
    }
}
