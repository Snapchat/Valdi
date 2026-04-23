package com.snap.valdi.modules.webview

import android.content.Context
import android.webkit.JavascriptInterface
import android.webkit.WebView
import android.webkit.WebViewClient
import com.snap.valdi.promise.Promise
import com.snap.valdi.promise.ResolvablePromise

internal class WebViewControllerImpl : IWebViewController, ValdiNativeWebViewController {
    private val listenerLock = Any()
    private lateinit var webViewHolder: AndroidWebViewHolder
    private var listener: IWebViewListener? = null
    private var loading = false

    init {
        webViewHolder = AndroidWebViewHolder(
            webViewClient = object : WebViewClient() {
                override fun onPageStarted(view: WebView, url: String?, favicon: android.graphics.Bitmap?) {
                    loading = true
                    webViewHolder.injectBridge()
                }

                override fun onPageFinished(view: WebView, url: String?) {
                    loading = false
                    webViewHolder.injectBridge()
                    lockedListener()?.onLoadCompleted()
                }

                @Deprecated("Deprecated in Java")
                override fun onReceivedError(
                    view: WebView,
                    errorCode: Int,
                    description: String?,
                    failingUrl: String?
                ) {
                    loading = false
                    lockedListener()?.onLoadFailed(description ?: "Failed to load $failingUrl")
                }
            },
            bridge = Bridge(this),
            bridgeName = BRIDGE_NAME,
            nativeBridgeName = NATIVE_BRIDGE_NAME
        )
    }

    override fun getWebView(context: Context): android.view.View? {
        return webViewHolder.getWebViewSync(context)
    }

    override fun load(request: WebViewLoadRequest) {
        webViewHolder.getWebViewOnUiThread { webView ->
            loadRequest(request, webView)
        }
    }

    override fun reload() {
        webViewHolder.getWebViewOnUiThread { webView ->
            loading = true
            webView.reload()
        }
    }

    override fun stopLoading() {
        webViewHolder.getWebViewOnUiThread { webView ->
            loading = false
            webView.stopLoading()
        }
    }

    override fun getState(): Promise<WebViewControllerState> {
        val promise = ResolvablePromise<WebViewControllerState>()
        webViewHolder.withExistingWebViewOnUiThread { webView ->
            promise.fulfillSuccess(
                WebViewControllerState(
                    webView?.canGoBack() ?: false,
                    webView?.canGoForward() ?: false,
                    loading
                )
            )
        }
        return promise
    }

    override fun goBack() {
        webViewHolder.getWebViewOnUiThread { webView ->
            webView.goBack()
        }
    }

    override fun goForward() {
        webViewHolder.getWebViewOnUiThread { webView ->
            webView.goForward()
        }
    }

    override fun evaluateJavaScript(script: String, callback: ((WebViewJavaScriptResult) -> Unit)?) {
        webViewHolder.getWebViewOnUiThread { webView ->
            webView.evaluateJavascript(script) { value ->
                callback?.invoke(WebViewJavaScriptResult(value, null))
            }
        }
    }

    override fun setListener(listener: IWebViewListener?) {
        setLockedListener(listener)
        webViewHolder.setBridgeEnabled(listener != null)
    }

    override fun dispose() {
        setLockedListener(null)
        loading = false
        webViewHolder.dispose()
    }

    private fun lockedListener(): IWebViewListener? {
        synchronized(listenerLock) {
            return listener
        }
    }

    private fun setLockedListener(listener: IWebViewListener?) {
        synchronized(listenerLock) {
            this.listener = listener
        }
    }

    private fun loadRequest(request: WebViewLoadRequest, webView: WebView) {
        val html = request.html
        if (html != null) {
            loading = true
            webView.loadDataWithBaseURL(null, html, "text/html", "UTF-8", null)
            return
        }

        val url = request.url
        if (url != null) {
            loading = true
            webView.loadUrl(url)
        }
    }

    private fun handleMessage(message: String) {
        lockedListener()?.onMessage(message)
    }

    private class Bridge(private val controller: WebViewControllerImpl) {
        @JavascriptInterface
        fun postMessage(message: String?) {
            controller.handleMessage(message ?: "")
        }
    }

    private companion object {
        private const val BRIDGE_NAME = "Valdi"
        private const val NATIVE_BRIDGE_NAME = "__ValdiNativeBridge"
    }
}
