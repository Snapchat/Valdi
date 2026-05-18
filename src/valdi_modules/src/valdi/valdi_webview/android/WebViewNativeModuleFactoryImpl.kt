package com.snap.valdi.modules.webview

import com.snap.valdi.modules.RegisterValdiModule

@RegisterValdiModule
class WebViewNativeModuleFactoryImpl : WebViewNativeModuleFactory() {
    override fun onLoadModule(): WebViewNativeModule {
        return object : WebViewNativeModule {
            override fun createNativeController(): IWebViewController {
                return WebViewControllerImpl()
            }
        }
    }
}
