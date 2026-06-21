package com.snap.valdi.attributes.impl

import android.content.Context
import android.view.ViewGroup
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.views.ValdiTextView

class ValdiTextViewAttributesBinder(
        private val context: Context
) : AttributesBinder<ValdiTextView> {
    override val viewClass: Class<ValdiTextView>
        get() = ValdiTextView::class.java

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiTextView>) {
        attributesBindingContext.setPlaceholderViewMeasureDelegate(lazy {
            val textView = ValdiTextView(context)
            textView.layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
            )
            textView
        })
    }
}
