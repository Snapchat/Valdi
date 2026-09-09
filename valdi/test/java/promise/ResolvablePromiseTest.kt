package com.snap.valdi.promise

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Covers the promise cancellation semantics — see the Promise::cancel contract in the C++
 * Promise.hpp.
 */
class ResolvablePromiseTest {

    private class TrackingCallback<T> : PromiseCallback<T> {
        var successValue: T? = null
        var failure: Throwable? = null
        var invocations = 0

        override fun onSuccess(value: T) {
            invocations += 1
            successValue = value
        }

        override fun onFailure(error: Throwable) {
            invocations += 1
            failure = error
        }
    }

    private class SettlingCancelablePromise<T>(
        private val onCancelAction: (SettlingCancelablePromise<T>) -> Unit
    ) : CancelableResolvablePromise<T>() {
        override fun onCancel() {
            onCancelAction(this)
        }
    }

    @Test
    fun `cancel forwards canceled failure to registered callbacks`() {
        val promise = ResolvablePromise<Int>()
        val callback = TrackingCallback<Int>()
        promise.onComplete(callback)

        promise.cancel()

        assertEquals(1, callback.invocations)
        assertTrue(callback.failure is PromiseCanceledException)
        assertNull(callback.successValue)
    }

    @Test
    fun `cancel forwards producer result when onCancel settles`() {
        val promise = SettlingCancelablePromise<Int> { it.fulfillSuccess(42) }
        val callback = TrackingCallback<Int>()
        promise.onComplete(callback)

        promise.cancel()

        assertEquals(1, callback.invocations)
        assertEquals(42, callback.successValue)
        assertNull("cancel must not mask the producer's real result", callback.failure)
    }

    @Test
    fun `cancel forwards producer failure when onCancel settles with error`() {
        val producerError = IllegalStateException("real failure")
        val promise = SettlingCancelablePromise<Int> { it.fulfillFailure(producerError) }
        val callback = TrackingCallback<Int>()
        promise.onComplete(callback)

        promise.cancel()

        assertEquals(1, callback.invocations)
        assertEquals(producerError, callback.failure)
    }

    @Test
    fun `cancel settles with canceled failure when onCancel does not settle`() {
        val promise = SettlingCancelablePromise<Int> { /* producer never settles */ }
        val callback = TrackingCallback<Int>()
        promise.onComplete(callback)

        promise.cancel()

        assertEquals(1, callback.invocations)
        assertTrue(callback.failure is PromiseCanceledException)
    }

    @Test
    fun `onComplete after cancel is served the canceled failure`() {
        val promise = ResolvablePromise<Int>()
        promise.cancel()

        val callback = TrackingCallback<Int>()
        promise.onComplete(callback)

        assertEquals("a late registrant on a canceled promise must not hang", 1, callback.invocations)
        assertTrue(callback.failure is PromiseCanceledException)
    }

    @Test
    fun `fulfill after cancel is dropped`() {
        val promise = ResolvablePromise<Int>()
        val callback = TrackingCallback<Int>()
        promise.onComplete(callback)

        promise.cancel()
        assertEquals(1, callback.invocations)

        promise.fulfillSuccess(42)

        assertEquals("a late producer settle after cancel must be dropped", 1, callback.invocations)
        val lateCallback = TrackingCallback<Int>()
        promise.onComplete(lateCallback)
        assertTrue("the recorded result should remain the canceled failure",
            lateCallback.failure is PromiseCanceledException)
    }

    @Test
    fun `cancel after fulfill is a no-op`() {
        val promise = ResolvablePromise<Int>()
        val callback = TrackingCallback<Int>()
        promise.onComplete(callback)

        promise.fulfillSuccess(42)
        promise.cancel()

        assertEquals(1, callback.invocations)
        assertEquals(42, callback.successValue)
        assertFalse(callback.failure is PromiseCanceledException)
    }

    @Test
    fun `onComplete between cancel phases is drained by finishCancel`() {
        // doCancel releases the monitor before onCancel runs, so a callback registered from inside
        // onCancel lands in completions with nothing recorded yet. finishCancel has to drain it,
        // otherwise it is retained forever and its awaiter hangs.
        val callback = TrackingCallback<Int>()
        val promise = SettlingCancelablePromise<Int> { it.onComplete(callback) }

        promise.cancel()

        assertEquals("a callback registered between cancel's two phases must not hang", 1, callback.invocations)
        assertTrue(callback.failure is PromiseCanceledException)
    }

    @Test
    fun `onCancel throwing still settles the promise`() {
        // onCancel is producer-supplied and can throw — the Rx-backed subclasses dispose a chain
        // from it. If that escapes before finishCancel, the promise stays canceled-but-unsettled
        // and its callbacks are never notified nor released.
        val producerFailure = IllegalStateException("dispose failed")
        val promise = SettlingCancelablePromise<Int> { throw producerFailure }
        val callback = TrackingCallback<Int>()
        promise.onComplete(callback)

        val thrown = try {
            promise.cancel()
            null
        } catch (e: IllegalStateException) {
            e
        }

        assertEquals("the producer's failure should still surface to the caller", producerFailure, thrown)
        assertEquals("a throwing onCancel must not leave the callbacks unsettled", 1, callback.invocations)
        assertTrue(callback.failure is PromiseCanceledException)

        val lateCallback = TrackingCallback<Int>()
        promise.onComplete(lateCallback)
        assertTrue("the promise should be settled, so late registrants are served",
            lateCallback.failure is PromiseCanceledException)
    }

}
