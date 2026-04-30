package com.snap.valdi.callable

import androidx.annotation.Keep
import com.snap.valdi.actions.ValdiAction
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.exceptions.GlobalExceptionHandler
import com.snap.valdi.exceptions.messageWithCauses
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.ValdiMarshallerCPP
import com.snap.valdi.utils.NativeHandlesManager
import com.snapchat.client.valdi.utils.ValdiCPPAction
import java.lang.Runnable

// TODO(simon): Remove inheritance from ValdiCPPAction once usage of it is removed.
@Keep
internal class ValdiFunctionNative(ptr: Long): ValdiCPPAction(ptr), ValdiFunction, ValdiAction {

    override fun perform(marshaller: ValdiMarshaller): Boolean {
        return perform(FLAGS_NONE, marshaller)
    }

    fun perform(flags: Int, marshaller: ValdiMarshaller): Boolean {
        return nativePerform(nativeHandle, flags, marshaller.nativeHandle)
    }

    fun performWithTimeout(flags: Int, marshaller: ValdiMarshaller, timeoutMs: Long): Boolean {
        return nativePerformWithTimeout(nativeHandle, flags, marshaller.nativeHandle, timeoutMs)
    }

    fun perform(flags: Int, parameters: Array<Any?>): Any? {
        return ValdiMarshaller.use {
            for (param in parameters) {
                it.pushUntyped(param)
            }

            val hasValue = perform(flags, it)

            if (hasValue) {
                it.getUntyped(-1)
            } else {
                null
            }
        }
    }

    override fun perform(parameters: Array<Any?>): Any? {
        return perform(FLAGS_NONE, parameters)
    }

    companion object {
        // Those flags should be kept in sync with Valdi::ValueFunction
        const val FLAGS_NONE = 0
        const val FLAGS_CALL_SYNC = 1 shl 0
        const val FLAGS_NEVER_CALL_SYNC = 1 shl 1
        const val FLAGS_ALLOW_THROTTLING = 1 shl 2
        const val FLAGS_PROPAGATES_ERROR = 1 shl 3

        @JvmStatic
        private external fun nativePerform(ptr: Long, flags: Int, marshallerHandle: Long): Boolean

        @JvmStatic
        private external fun nativePerformWithTimeout(ptr: Long, flags: Int, marshallerHandle: Long, timeoutMs: Long): Boolean

        @JvmStatic
        fun performFromNative(function: ValdiFunction, marshallerHandle: Long) {
//            println("[Valdi] ${Thread.currentThread().name} CALL JS FUNCTION $function")
            ValdiMarshallerCPP.javaEntry(marshallerHandle) {
                try {
                    function.perform(it)
                } catch (exception: ValdiException) {
                    it.setError(exception.messageWithCauses())
                }
                null
            }
        }

        @JvmStatic
        fun performRunnableFromNative(runnable: Runnable) {
            try {
                runnable.run()
            } catch (throwable: Throwable) {
                GlobalExceptionHandler.onFatalException(throwable)
            }
        }

    }
}
