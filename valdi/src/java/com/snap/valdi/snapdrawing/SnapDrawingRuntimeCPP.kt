package com.snap.valdi.snapdrawing

import android.content.Context
import com.snap.valdi.attributes.impl.fonts.FontStyle
import com.snap.valdi.attributes.impl.fonts.FontWeight
import com.snap.valdi.utils.NativeRef
import com.snap.valdi.utils.ViewRefSupport
import com.snapchat.client.valdi.NativeBridge

class SnapDrawingRuntimeCPP(private val handle: Lazy<NativeRef>,
                            private var displayScale: Float,
                            private val viewRefSupport: ViewRefSupport,
                            private val context: Context): SnapDrawingRuntime {

    override fun getNativeHandle(): Long {
        return handle.value.nativeHandle
    }

    private var surfacePresenterFactory: SurfacePresenterFactory? = null

    fun clearCache() {
        surfacePresenterFactory?.clearCache()
    }

    fun updateDisplayScale(newScale: Float) {
        displayScale = newScale
    }

    override fun createRoot(disallowSynchronousDraw: Boolean): SnapDrawingRootHandle {
        return SnapDrawingRootHandle(NativeBridge.createSnapDrawingRoot(getNativeHandle(), disallowSynchronousDraw), displayScale)
    }

    override fun getSurfacePresenterFactory(): SurfacePresenterFactory {
        var surfacePresenterFactory = this.surfacePresenterFactory
        if (surfacePresenterFactory == null) {
            surfacePresenterFactory = SurfacePresenterFactory(viewRefSupport.bitmapPool, context)
            this.surfacePresenterFactory = surfacePresenterFactory
        }

        return surfacePresenterFactory
    }

    override fun registerTypeface(familyName: String,
                                  fontWeight: FontWeight,
                                  fontStyle: FontStyle,
                                  isFallback: Boolean,
                                  typefaceBytes: ByteArray?,
                                  typefaceFd: Int?) {
        NativeBridge.snapDrawingRegisterTypeface(getNativeHandle(),
                familyName,
                fontWeight.toString(),
                fontStyle.toString(),
                isFallback,
                typefaceBytes,
                typefaceFd ?: 0)
    }

    override fun getMaxRenderTargetSize(): Long {
        return NativeBridge.snapDrawingGetMaxRenderTargetSize(getNativeHandle())
    }

}
