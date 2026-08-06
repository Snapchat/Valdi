package com.snap.valdi.test.adapters

import android.view.View
import androidx.test.espresso.PerformException
import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import androidx.test.espresso.matcher.ViewMatchers.isRoot
import androidx.test.espresso.util.HumanReadables
import androidx.test.espresso.util.TreeIterables
import org.hamcrest.Matcher

private const val POLL_INTERVAL_MS = 50L

/**
 * Espresso ViewAction that polls the view tree rooted at the matched root view until [matcher]
 * matches a view in it (or, if [waitForAbsence], until it no longer does), up to [timeoutMs].
 */
class EspressoWaitForViewAction private constructor(
    private val matcher: Matcher<View>,
    private val timeoutMs: Long,
    private val waitForAbsence: Boolean
) : ViewAction {

    companion object {
        @JvmStatic
        fun waitForViewUpTo(matcher: Matcher<View>, timeoutMs: Long): ViewAction =
            EspressoWaitForViewAction(matcher, timeoutMs, waitForAbsence = false)

        @JvmStatic
        fun waitForViewToDisappearUpTo(matcher: Matcher<View>, timeoutMs: Long): ViewAction =
            EspressoWaitForViewAction(matcher, timeoutMs, waitForAbsence = true)
    }

    override fun getConstraints(): Matcher<View> = isRoot()

    override fun getDescription(): String {
        val verb = if (waitForAbsence) "disappear" else "appear"
        return "wait up to $timeoutMs ms for a view to $verb matching $matcher"
    }

    override fun perform(uiController: UiController, view: View) {
        uiController.loopMainThreadUntilIdle()
        val deadline = System.currentTimeMillis() + timeoutMs

        while (true) {
            val found = TreeIterables.breadthFirstViewTraversal(view).any { matcher.matches(it) }
            if (found != waitForAbsence) {
                return
            }

            if (System.currentTimeMillis() >= deadline) {
                val verb = if (waitForAbsence) "disappear" else "appear"
                throw PerformException.Builder()
                    .withActionDescription(description)
                    .withViewDescription(HumanReadables.describe(view))
                    .withCause(RuntimeException("Timed out after $timeoutMs ms waiting for view to $verb"))
                    .build()
            }

            uiController.loopMainThreadForAtLeast(POLL_INTERVAL_MS)
        }
    }
}
