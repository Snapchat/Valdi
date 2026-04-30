package com.snapchat.client.valdi.utils

import androidx.annotation.Keep
import com.snap.valdi.utils.NativeHandlesManager
import com.snap.valdi.utils.Ref
import com.snap.valdi.schema.ValdiUntypedClass
import com.snapchat.client.valdi.NativeBridge

@ValdiUntypedClass
@Keep
open class CppObjectWrapper(nativeHandle: Long) : NativeHandleWrapper(nativeHandle), Ref {

    private val destroyDisabled: Boolean = NativeHandlesManager.add(this)

    override fun destroyHandle(handle: Long) {
        if (destroyDisabled) {
            return
        }
        NativeBridge.deleteNativeHandle(handle)
    }

    override fun get(): Any {
        return nativeHandle
    }
}

