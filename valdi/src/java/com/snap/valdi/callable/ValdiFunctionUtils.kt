package com.snap.valdi.callable

import com.snap.valdi.actions.ValdiAction
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.utils.ValdiMarshaller

fun ValdiFunction.performSync(marshaller: ValdiMarshaller, propagatesError: Boolean): Boolean {
    return if (this is ValdiFunctionNative) {
        val flags = if (propagatesError) {
            ValdiFunctionNative.FLAGS_CALL_SYNC or ValdiFunctionNative.FLAGS_PROPAGATES_ERROR
        } else {
            ValdiFunctionNative.FLAGS_CALL_SYNC
        }
        return this.perform(flags, marshaller)
    } else {
        this.perform(marshaller)
    }
}

/**
 * Performs a synchronous call to the function with a timeout.
 * If the call does not complete within the specified timeout, returns false.
 * 
 * @param marshaller The marshaller containing the parameters
 * @param propagatesError Whether to propagate errors from the function
 * @param timeoutMs Maximum time to wait for the function to complete (in milliseconds)
 * @return true if the function completed successfully and returned a value, false otherwise
 */
fun ValdiFunction.performSyncWithTimeout(
    marshaller: ValdiMarshaller,
    propagatesError: Boolean,
    timeoutMs: Long
): Boolean {
    return if (this is ValdiFunctionNative) {
        val flags = if (propagatesError) {
            ValdiFunctionNative.FLAGS_CALL_SYNC or ValdiFunctionNative.FLAGS_PROPAGATES_ERROR
        } else {
            ValdiFunctionNative.FLAGS_CALL_SYNC
        }
        return this.performWithTimeout(flags, marshaller, timeoutMs)
    } else {
        // Fallback to regular sync call for non-native functions
        this.perform(marshaller)
    }
}

fun ValdiFunction.performThrottled(marshaller: ValdiMarshaller): Boolean {
    return if (this is ValdiFunctionNative) {
        this.perform(ValdiFunctionNative.FLAGS_ALLOW_THROTTLING, marshaller)
    } else {
        this.perform(marshaller)
    }
}

/**
 * Convenience function to call a ValdiFunction with arguments passed as an array.
 * Due to the temporary array and the value boxing, this is slower than using the perform()
 * and passing the parameters through the Marshaller directly.
 */
fun ValdiFunction.performUntyped(parameters: Array<Any?>): Any? {
    return ValdiMarshaller.use { marshaller ->
        parameters.forEach { marshaller.pushUntyped(it) }

        if (perform(marshaller)) {
            marshaller.getUntyped(-1)
        } else {
            null
        }
    }
}

fun ValdiFunction.dispose() {
    if (this is ValdiFunctionNative) {
        this.destroy()
    }
}

fun ValdiAction.toFunction(): ValdiFunction {
    return if (this is ValdiFunction) {
        this
    } else {
        ValdiFunctionActionAdapter(this)
    }
}

fun ValdiFunction.toAction(): ValdiAction {
    return if (this is ValdiAction) {
        this
    } else if (this is ValdiFunctionActionAdapter) {
        this.action
    } else {
        // No support for converting to Action.
        // This isn't really necessary for now.
        throw ValdiException("Converting from $this to ValdiAction is not yet supported.")
    }
}
