package com.snap.valdi

import androidx.annotation.Keep

@Keep
interface ViewFactoryPrivate {

    @Keep
    fun createView(valdiContext: Any?): ViewRef

    @Keep
    fun bindAttributes(bindingContextHandle: Long)

    @Keep
    fun managesChildFrames(): Boolean

}
