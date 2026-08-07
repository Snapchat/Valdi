package com.snap.valdi.attributes.impl

import android.content.Context
import android.view.ViewGroup
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextViewMeasureDelegate
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.ValdiTextView

class ValdiTextViewAttributesBinder(
        private val context: Context,
        private val fontManager: FontManager,
        private val defaultAttributes: FontAttributes,
        private val logger: Logger,
        private val enableDirectTextViewMeasure: Boolean,
) : AttributesBinder<ValdiTextView> {
    override val viewClass: Class<ValdiTextView>
        get() = ValdiTextView::class.java

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiTextView>) {
        if (enableDirectTextViewMeasure) {
            attributesBindingContext.setMeasureDelegate(
                    TextViewMeasureDelegate(fontManager, defaultAttributes, logger)
            )
        } else {
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
}
