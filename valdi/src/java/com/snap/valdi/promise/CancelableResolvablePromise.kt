package com.snap.valdi.promise

abstract class CancelableResolvablePromise<T>: ResolvablePromise<T>() {

    protected abstract fun onCancel()

    override fun cancel() {
        if (doCancel()) {
            // onCancel may settle the promise synchronously, in which case the callbacks receive
            // the producer's real result and finishCancel is a no-op.
            //
            // finally: onCancel is producer-supplied and can throw — subclasses dispose Rx chains
            // from it. Letting that escape would leave the promise canceled but unsettled, so the
            // callbacks would never be notified nor released, which is the hang this class exists
            // to avoid.
            try {
                onCancel()
            } finally {
                finishCancel()
            }
        }
    }

    override fun isCancelable(): Boolean {
        return true
    }
}