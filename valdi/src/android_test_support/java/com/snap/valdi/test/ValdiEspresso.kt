package com.snap.valdi.test

import android.view.View
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.PerformException
import androidx.test.espresso.ViewAction
import androidx.test.espresso.matcher.ViewMatchers.isRoot
import com.snap.valdi.test.adapters.EspressoViewMatcherAdapter
import com.snap.valdi.test.adapters.EspressoWaitForViewAction.Companion.waitForViewToDisappearUpTo
import com.snap.valdi.test.adapters.EspressoWaitForViewAction.Companion.waitForViewUpTo
import com.snap.valdi.test.utils.CapturedValdiElementWithRootView
import org.hamcrest.Matcher
import kotlin.time.Duration

private const val DEFAULT_MAX_RETRIES = 30
private const val DEFAULT_WAIT_INTERVAL_MS = 1000L

object ValdiEspresso {
    @JvmStatic
    fun onValdiElement(matcher: Matcher<ValdiElementWithRootView>): ValdiElementInteraction {
        return ValdiElementInteraction(matcher)
    }

    @JvmStatic
    fun waitUntil(matcher: Matcher<ValdiElementWithRootView>) {
        waitUntil(DEFAULT_MAX_RETRIES, DEFAULT_WAIT_INTERVAL_MS, matcher)
    }

    @JvmStatic
    fun waitUntil(maxRetries: Int, waitInterval: Long, matcher: Matcher<ValdiElementWithRootView>) {
        retryOnRoot(maxRetries) { waitForViewUpTo(adapt(matcher), waitInterval) }
    }

    @JvmStatic
    fun waitUntil(maxRetries: Int, waitInterval: Duration, matcher: Matcher<ValdiElementWithRootView>) {
        waitUntil(maxRetries, waitInterval.inWholeMilliseconds, matcher)
    }

    @JvmStatic
    fun waitUntilDisplayed(matcher: Matcher<ValdiElementWithRootView>) {
        waitUntil(matcher)
    }

    @JvmStatic
    fun waitUntilDisplayed(maxRetries: Int, waitInterval: Long, matcher: Matcher<ValdiElementWithRootView>) {
        waitUntil(maxRetries, waitInterval, matcher)
    }

    @JvmStatic
    fun waitUntilDisappear(matcher: Matcher<ValdiElementWithRootView>) {
        try {
            retryOnRoot(DEFAULT_MAX_RETRIES) {
                waitForViewToDisappearUpTo(adapt(matcher), DEFAULT_WAIT_INTERVAL_MS)
            }
        } catch (e: PerformException) {
            // Matches the original crema behavior: give up quietly if the view never disappears
            // within the retry budget, rather than failing the caller.
        }
    }

    @JvmStatic
    private fun adapt(matcher: Matcher<ValdiElementWithRootView>): Matcher<View> {
        return EspressoViewMatcherAdapter(CapturedValdiElementWithRootView(), matcher)
    }

    /**
     * Runs [action] as a series of separate onView(isRoot()).perform() calls rather than one
     * long-lived perform(), so Espresso re-syncs on registered IdlingResources between attempts
     * instead of just pumping the looper for a fixed duration inside a single perform() call.
     */
    private fun retryOnRoot(maxAttempts: Int, action: () -> ViewAction) {
        var lastError: PerformException? = null
        for (attempt in 1..maxAttempts) {
            try {
                onView(isRoot()).perform(action())
                return
            } catch (e: PerformException) {
                lastError = e
            }
        }
        throw lastError ?: IllegalStateException("waitUntil retries exhausted with no attempts made")
    }
}
