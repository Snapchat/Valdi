package com.snap.valdi.modules.webview

import android.annotation.SuppressLint
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.view.View
import android.view.ViewGroup
import android.webkit.WebChromeClient
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import java.util.concurrent.CountDownLatch

internal class AndroidWebViewHolder(
    private val webViewClient: WebViewClient,
    private val bridge: Any,
    private val bridgeName: String,
    private val nativeBridgeName: String
) {
    private val mainHandler = Handler(Looper.getMainLooper())
    private val pendingCallbacks = mutableListOf<(WebView) -> Unit>()
    private var context: Context? = null
    private var webView: WebView? = null
    private var disposed = false
    private var bridgeEnabled = false
    private var bridgeInstalled = false

    fun getWebViewSync(context: Context): View? {
        if (isUiThread()) {
            this.context = context
            return ensureWebView()
        }

        var result: View? = null
        val latch = CountDownLatch(1)
        mainHandler.post {
            this.context = context
            result = ensureWebView()
            latch.countDown()
        }
        latch.await()
        return result
    }

    fun getWebViewOnUiThread(callback: (WebView) -> Unit) {
        runOnUiThread {
            val webView = ensureWebView()
            if (webView == null) {
                if (!disposed) {
                    pendingCallbacks.add(callback)
                }
                return@runOnUiThread
            }
            callback(webView)
        }
    }

    fun withExistingWebViewOnUiThread(callback: (WebView?) -> Unit) {
        runOnUiThread {
            callback(webView)
        }
    }

    fun setBridgeEnabled(bridgeEnabled: Boolean) {
        runOnUiThread {
            this.bridgeEnabled = bridgeEnabled
            webView?.let { configureBridge(it) }
        }
    }

    fun injectBridge() {
        runOnUiThread {
            webView?.let { injectBridge(it) }
        }
    }

    fun dispose() {
        runOnUiThread {
            disposed = true
            context = null
            pendingCallbacks.clear()
            webView?.let {
                prepareWebViewForRelease(it)
                it.destroy()
            }
            webView = null
        }
    }

    private fun ensureWebView(): WebView? {
        if (disposed) {
            return null
        }

        webView?.let {
            flushPendingCallbacks(it)
            return it
        }

        val context = context ?: return null
        return WebView(context).also {
            webView = it
            configureWebView(it)
            flushPendingCallbacks(it)
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    private fun configureWebView(webView: WebView) {
        webView.isFocusable = true
        webView.isFocusableInTouchMode = true
        webView.webChromeClient = WebChromeClient()
        configureSettings(webView.settings)
        webView.webViewClient = webViewClient
        configureBridge(webView)
    }

    private fun configureSettings(settings: WebSettings) {
        settings.javaScriptEnabled = true
        settings.domStorageEnabled = true
        settings.javaScriptCanOpenWindowsAutomatically = true
        settings.loadsImagesAutomatically = true
        settings.mediaPlaybackRequiresUserGesture = false
        settings.useWideViewPort = true
        settings.loadWithOverviewMode = true
    }

    private fun configureBridge(webView: WebView) {
        if (!bridgeEnabled) {
            if (bridgeInstalled) {
                webView.removeJavascriptInterface(nativeBridgeName)
                bridgeInstalled = false
            }
            return
        }

        if (!bridgeInstalled) {
            webView.addJavascriptInterface(bridge, nativeBridgeName)
            bridgeInstalled = true
        }
        injectBridge(webView)
    }

    private fun injectBridge(webView: WebView) {
        if (!bridgeEnabled || !bridgeInstalled) {
            return
        }

        webView.evaluateJavascript(
            """
            window.$bridgeName = window.$bridgeName || {};
            window.$bridgeName.postMessage = function(message) {
              var stringMessage = typeof message === 'string' ? message : JSON.stringify(message);
              window.$nativeBridgeName.postMessage(String(stringMessage));
            };
            """.trimIndent(),
            null
        )
    }

    private fun prepareWebViewForRelease(webView: WebView) {
        (webView.parent as? ViewGroup)?.removeView(webView)
        webView.stopLoading()
        if (bridgeInstalled) {
            webView.removeJavascriptInterface(nativeBridgeName)
            bridgeInstalled = false
        }
        webView.webViewClient = WebViewClient()
        webView.webChromeClient = WebChromeClient()
    }

    private fun flushPendingCallbacks(webView: WebView) {
        if (pendingCallbacks.isEmpty()) {
            return
        }

        val callbacks = pendingCallbacks.toList()
        pendingCallbacks.clear()
        callbacks.forEach { it(webView) }
    }

    private fun runOnUiThread(block: () -> Unit) {
        if (isUiThread()) {
            block()
            return
        }
        mainHandler.post(block)
    }

    private fun isUiThread(): Boolean {
        return Looper.myLooper() == Looper.getMainLooper()
    }
}
