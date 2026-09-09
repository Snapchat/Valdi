package com.snap.valdi.promise

import com.snap.valdi.utils.DisposableUtils
import java.util.concurrent.CancellationException

/**
 * Failure delivered to a canceled promise's callbacks when the producer does not settle it during
 * cancellation (see [Promise.cancel] contract in the C++ Promise.hpp). Mirrors the C++
 * kPromiseCanceledErrorCode failure so consumers can distinguish cancellation from a genuine error.
 *
 * Being a [CancellationException] is what carries the code across the JNI boundary:
 * `Throwable.valdiErrorCode()` maps it to `PROMISE_CANCELED_ERROR_CODE`, which
 * `CppPromiseCallback.onFailure` passes to native alongside the message. In the other direction a
 * coded native failure arrives as a `ValdiException` with `errorCode` set.
 */
class PromiseCanceledException : CancellationException("Promise canceled")

open class ResolvablePromise<T>: Promise<T> {

    private var completions: MutableList<PromiseCallback<T>>? = null
    private var completed = false
    private var error: Throwable? = null
    private var value: T? = null
    private var canceled = false

    fun fulfillSuccess(value: T) {
        var completions: MutableList<PromiseCallback<T>>?
        synchronized(this) {
            // A producer may legitimately settle after cancel — its synchronous settle from
            // onCancel is what forwards the real result; only a completed promise drops the result.
            if (completed) {
                return
            }
            completed = true
            this.value = value
            completions = this.completions
            this.completions = null
        }

        completions?.forEach {
            it.onSuccess(value)
            DisposableUtils.disposeAny(it)
        }
    }

    fun fulfillFailure(error: Throwable) {
        var completions: MutableList<PromiseCallback<T>>?
        synchronized(this) {
            if (completed) {
                return
            }
            completed = true
            this.error = error
            completions = this.completions
            this.completions = null
        }

        completions?.forEach {
            it.onFailure(error)
            DisposableUtils.disposeAny(it)
        }
    }

    override fun onComplete(callback: PromiseCallback<T>) {
        var error: Throwable?
        var value: T?
        synchronized(this) {
            // A registrant that lands between doCancel and finishCancel is appended and drained
            // by finishCancel or by a synchronous producer settle.
            if (!completed) {
                var completions = this.completions
                if (completions == null) {
                    completions = mutableListOf()
                    this.completions = completions
                }
                completions.add(callback)
                return
            } else {
                error = this.error
                value = this.value
                // We are already completed
                // Unlock the mutex and call the completion with the result
            }
        }

        if (error != null) {
            callback.onFailure(error!!)
        } else {
            callback.onSuccess(value!!)
        }
        DisposableUtils.disposeAny(callback)
    }

    override fun cancel() {
        if (doCancel()) {
            finishCancel()
        }
    }

    override fun isCancelable(): Boolean {
        return false
    }

    protected fun doCancel(): Boolean {
        synchronized(this) {
            if (canceled || completed) {
                return false
            }
            this.canceled = true
        }
        return true
    }

    /**
     * Second phase of cancellation, run after the producer's cancel hook had a chance to settle the
     * promise: if it did, the callbacks already received the real result; otherwise settle with
     * [PromiseCanceledException] and forward it to (and release) the pending callbacks.
     */
    protected fun finishCancel() {
        var completions: MutableList<PromiseCallback<T>>?
        val error: Throwable
        synchronized(this) {
            if (completed) {
                return
            }
            error = PromiseCanceledException()
            this.error = error
            completed = true
            completions = this.completions
            this.completions = null
        }

        completions?.forEach {
            it.onFailure(error)
            DisposableUtils.disposeAny(it)
        }
    }

}
