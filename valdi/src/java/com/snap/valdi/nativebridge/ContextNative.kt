package com.snap.valdi.nativebridge

import com.snap.valdi.ValdiRuntime
import com.snap.valdi.ViewRef
import com.snapchat.client.valdi.NativeBridge
import com.snap.valdi.utils.NativeRef
/**
 * Bridge class for a native Context, which forwards calls to the NativeBridge.
 */
class ContextNative(nativeHandle: Long, val nativeBridge: NativeBridge, val runtimeNative: RuntimeNative) {

    /**
     * Owns the strong reference to the native Context. Calls made through [withHandle] pin it, so that a
     * concurrent [destroy] or [markDestroyed] on another thread defers releasing that reference until the
     * last in-flight call returns. Releasing it between reading the handle and the native side retaining
     * it would hand native a pointer to a Context whose last reference is gone.
     */
    open class PinnableContextRef(handle: Long) : NativeRef(handle) {
        private var pinCount = 0
        private var destroyRequested = false

        override fun getNativeHandle(): Long {
            synchronized(this) {
                return if (destroyRequested) 0L else super.getNativeHandle()
            }
        }

        /**
         * Returns the handle with a pin held, or 0 if the reference is already destroyed. Every non-zero
         * return must be balanced by [unpin].
         */
        fun pin(): Long {
            synchronized(this) {
                if (destroyRequested) {
                    return 0L
                }
                val handle = super.getNativeHandle()
                if (handle != 0L) {
                    pinCount++
                }
                return handle
            }
        }

        fun unpin() {
            val releaseNow = synchronized(this) {
                pinCount--
                pinCount == 0 && destroyRequested
            }
            if (releaseNow) {
                super.destroy()
            }
        }

        override fun destroy() {
            val releaseNow = synchronized(this) {
                destroyRequested = true
                pinCount == 0
            }
            if (releaseNow) {
                super.destroy()
            }
        }
    }

    val nativeWrapper = PinnableContextRef(nativeHandle)

    /**
     * Runs [block] with the native Context handle, guaranteeing the Context stays retained for the duration
     * of the call. Returns [defaultValue] without calling [block] when the Context was already destroyed.
     */
    inline fun <T> withHandle(defaultValue: T, block: (Long) -> T): T {
        val handle = nativeWrapper.pin()
        if (handle == 0L) {
            return defaultValue
        }
        try {
            return block(handle)
        } finally {
            nativeWrapper.unpin()
        }
    }

    inline fun withHandle(block: (Long) -> Unit) {
        withHandle(Unit, block)
    }

    fun onCreate() {
        withHandle { NativeBridge.contextOnCreate(it) }
    }

    fun destroy() {
        withHandle { NativeBridge.destroyContext(runtimeNative.nativeHandle, it) }
        nativeWrapper.destroy()
    }

    fun markDestroyed() {
        nativeWrapper.destroy()
    }

    fun getView(id: String): ViewRef? {
        return withHandle(null) { NativeBridge.getViewInContextForId(it, id) as? ViewRef }
    }

    fun setRootView(rootView: ViewRef?) {
        withHandle { NativeBridge.setRootView(runtimeNative.nativeHandle, it, rootView) }
    }

    fun setSnapDrawingRootView(snapDrawingRootHandle: Long, previousSnapDrawingRootHandle: Long, rootView: ViewRef?) {
        withHandle { NativeBridge.setSnapDrawingRootView(it, snapDrawingRootHandle, previousSnapDrawingRootHandle, rootView) }
    }

    fun setViewModel(viewModel: Any?) {
        withHandle { NativeBridge.setViewModel(it, viewModel) }
    }

    fun callJSFunction(functionName: String, arguments: Array<Any?>) {
        withHandle { NativeBridge.callJSFunction(runtimeNative.nativeHandle, it, functionName, arguments) }
    }

    fun measureLayout(maxWidth: Int, widthMode: Int, maxHeight: Int, heightMode: Int, isRTL: Boolean): Long {
        return withHandle(0L) {
            NativeBridge.measureLayout(runtimeNative.nativeHandle, it, maxWidth, widthMode, maxHeight, heightMode, isRTL)
        }
    }

    fun setLayoutSpecs(width: Int, height: Int, isRTL: Boolean) {
        withHandle { NativeBridge.setLayoutSpecs(runtimeNative.nativeHandle, it, width, height, isRTL) }
    }

    fun setVisibleViewport(x: Int, y: Int, width: Int, height: Int) {
        withHandle { NativeBridge.setVisibleViewport(runtimeNative.nativeHandle, it, x, y, width, height, false) }
    }

    fun unsetVisibleViewport() {
        withHandle { NativeBridge.setVisibleViewport(runtimeNative.nativeHandle, it, 0, 0, 0, 0, true) }
    }

    fun getRuntime(): ValdiRuntime? {
        return withHandle(null) { NativeBridge.getRuntimeAttachedObjectFromContext(it) as? ValdiRuntime }
    }

}
