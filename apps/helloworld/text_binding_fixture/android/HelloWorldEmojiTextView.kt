package com.snap.valdi.helloworld.textbinding

import android.content.Context
import android.widget.TextView
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.TextViewUtils
import com.snap.valdi.views.ValdiRecyclableView
import com.snap.valdi.views.ValdiTextBindableView
import com.snap.valdi.views.ValdiTextHolder

/**
 * An emoji-style text view: a plain platform [TextView] subclass, outside the
 * ValdiTextViewBase hierarchy, that receives Valdi text attributes only because it implements
 * [ValdiTextBindableView] and has its binder registered. Apps ship views like this to render text
 * with their own emoji handling. It implements the same Valdi interfaces as an app emoji view.
 */
class HelloWorldEmojiTextView(context: Context) :
    TextView(context), ValdiTextHolder, ValdiRecyclableView, ValdiTextBindableView {

    override var textViewHelper: TextViewHelper? = null
    override var onSelectionChangeFunction: ValdiFunction? = null

    override val bindingTextView: TextView
        get() = this

    init {
        TextViewUtils.configure(this)
    }

    override fun getOrCreateTextViewHelper(
        fontManager: FontManager,
        defaultAttributes: FontAttributes,
        valueAttributeId: Int,
        logger: Logger,
    ): TextViewHelper {
        return textViewHelper
            ?: TextViewHelper(this, fontManager, defaultAttributes, valueAttributeId, logger, this)
                .also { textViewHelper = it }
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        textViewHelper?.onMeasure(widthMeasureSpec, heightMeasureSpec)
        super.onMeasure(widthMeasureSpec, TextViewUtils.resolveHeightMeasureSpec(this, heightMeasureSpec))
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        textViewHelper?.onLayout(changed)
        super.onLayout(changed, left, top, right, bottom)
    }

    override fun prepareForRecycling() {
        textViewHelper?.prepareForRecycling()
    }

    override fun setTextAccessibility(text: CharSequence?) {
        super.setText(text, BufferType.NORMAL)
    }

    override fun setValdiSelectable(selectable: Boolean) {}

    override fun setValdiSelection(start: Int, end: Int) {}

    override fun refreshInlineTextAnimation() {}
}
