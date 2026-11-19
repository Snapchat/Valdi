package com.snap.valdi.helloworld.textbinding

import android.content.Context
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.AbstractTextViewAttributesBinder
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.logger.Logger

/**
 * Attribute binder for [HelloWorldTextView]: the shared Valdi text attributes (`value`,
 * `fontAttributes`, `textShadow`, `textOverflow`, `textGradient`) plus one attribute of its own,
 * `placeholder`, the way app binders extend the runtime's text binder.
 */
class HelloWorldTextViewAttributesBinder(
    context: Context,
    fontManager: FontManager,
    defaultAttributes: FontAttributes,
    logger: Logger,
) : AbstractTextViewAttributesBinder<HelloWorldTextView>(context, fontManager, defaultAttributes, logger) {

    override val viewClass: Class<HelloWorldTextView>
        get() = HelloWorldTextView::class.java

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<HelloWorldTextView>) {
        bindTextAttributes(attributesBindingContext)
        attributesBindingContext.bindStringAttribute("placeholder", true, this::applyPlaceholder, this::resetPlaceholder)
    }

    private fun applyPlaceholder(view: HelloWorldTextView, value: String, animator: ValdiAnimator?) {
        view.bindingTextView.hint = value
    }

    private fun resetPlaceholder(view: HelloWorldTextView, animator: ValdiAnimator?) {
        view.bindingTextView.hint = null
    }
}
