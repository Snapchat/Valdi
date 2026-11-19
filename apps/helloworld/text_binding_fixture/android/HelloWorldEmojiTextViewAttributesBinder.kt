package com.snap.valdi.helloworld.textbinding

import android.content.Context
import android.view.ViewGroup
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.AbstractTextViewAttributesBinder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.logger.Logger

/**
 * Binds the shared Valdi text attributes onto [HelloWorldEmojiTextView], plus the placeholder
 * measure delegate used to size text before a view instance exists.
 */
class HelloWorldEmojiTextViewAttributesBinder(
    private val context: Context,
    fontManager: FontManager,
    defaultAttributes: FontAttributes,
    logger: Logger,
) : AbstractTextViewAttributesBinder<HelloWorldEmojiTextView>(context, fontManager, defaultAttributes, logger) {

    override val viewClass: Class<HelloWorldEmojiTextView>
        get() = HelloWorldEmojiTextView::class.java

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<HelloWorldEmojiTextView>) {
        attributesBindingContext.setPlaceholderViewMeasureDelegate(
            lazy {
                val textView = HelloWorldEmojiTextView(context)
                textView.layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                )
                textView
            }
        )

        bindTextAttributes(attributesBindingContext)
    }
}
