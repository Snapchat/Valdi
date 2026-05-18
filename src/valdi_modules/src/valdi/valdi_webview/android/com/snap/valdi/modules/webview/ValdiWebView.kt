package com.snap.valdi.modules.webview

import android.content.Context
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.annotation.Keep
import com.snap.valdi.views.ValdiRecyclableView
import com.snap.valdi.views.ValdiTouchEventResult
import com.snap.valdi.views.ValdiTouchTarget

interface ValdiNativeWebViewController {
    fun getWebView(context: Context): View?
}

@Keep
open class ValdiWebView(context: Context) : FrameLayout(context), ValdiTouchTarget, ValdiRecyclableView {
    private var controller: ValdiNativeWebViewController? = null
    private var controllerWebView: View? = null

    fun setController(controller: ValdiNativeWebViewController?) {
        if (this.controller == controller) {
            return
        }
        detachControllerWebView()
        this.controller = controller
        attachControllerWebView()
    }

    override fun processTouchEvent(event: MotionEvent): ValdiTouchEventResult {
        if (dispatchTouchEvent(event)) {
            return ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        }
        return ValdiTouchEventResult.IgnoreEvent
    }

    override fun prepareForRecycling() {
        detachControllerWebView()
        controller = null
    }

    private fun attachControllerWebView() {
        val webView = controller?.getWebView(context) ?: return
        (webView.parent as? ViewGroup)?.removeView(webView)
        addView(webView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
        controllerWebView = webView
    }

    private fun detachControllerWebView() {
        val webView = controllerWebView
        if (webView != null && webView.parent == this) {
            removeView(webView)
        }
        controllerWebView = null
    }
}
