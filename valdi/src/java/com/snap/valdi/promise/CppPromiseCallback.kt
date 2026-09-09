package com.snap.valdi.promise

import androidx.annotation.Keep
import com.snap.valdi.exceptions.messageWithCauses
import com.snap.valdi.exceptions.valdiErrorCode
import com.snap.valdi.utils.CppNativeHandlePair

@Keep
class CppPromiseCallback<T>(nativeHandle: Long, valueMarshallerNativeHandle: Long):
    CppNativeHandlePair(nativeHandle, valueMarshallerNativeHandle), PromiseCallback<T> {

    override fun onSuccess(value: T) {
        // We swap because our native call is going to release the handle for us.
        nativeOnSuccess(this.swapNativeHandle1(), this.swapNativeHandle2(), value)
    }

    override fun onFailure(error: Throwable) {
        // The error code travels alongside the message so native (and JS beyond it) can tell an
        // intentional cancellation apart from a genuine failure without matching on text.
        nativeOnFailure(
            this.swapNativeHandle1(),
            this.swapNativeHandle2(),
            error.messageWithCauses(),
            error.valdiErrorCode(),
        )
    }

    companion object {
        @JvmStatic
        private external fun nativeOnSuccess(nativeHandle: Long, valueMarshallerNativeHandle: Long, value: Any?)
        @JvmStatic
        private external fun nativeOnFailure(
            nativeHandle: Long,
            valueMarshallerNativeHandle: Long,
            error: String,
            errorCode: Int,
        )
    }
}