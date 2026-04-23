package com.snap.valdi.modules.webview

import android.content.Context
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.RegisterAttributesBinder

@RegisterAttributesBinder
class ValdiWebViewAttributesBinder(private val context: Context) : AttributesBinder<ValdiWebView> {
    override val viewClass: Class<ValdiWebView>
        get() = ValdiWebView::class.java

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiWebView>) {
        attributesBindingContext.bindUntypedAttribute(
            "controller",
            false,
            { view, value ->
                view.setController(value as? ValdiNativeWebViewController)
            },
            { view ->
                view.setController(null)
            }
        )
    }
}
