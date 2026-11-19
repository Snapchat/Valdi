package com.snap.valdi.helloworld.textbinding

import android.content.Context
import com.snap.valdi.views.ValdiLabelBackingTextView
import com.snap.valdi.views.ValdiTextViewBase

/**
 * A host-app label built the way app code builds its own Valdi text views: it subclasses
 * [ValdiTextViewBase] around the runtime's [ValdiLabelBackingTextView], and is driven by
 * [HelloWorldTextViewAttributesBinder].
 */
class HelloWorldTextView(context: Context) :
    ValdiTextViewBase(context, ValdiLabelBackingTextView(context)) {

    val text: CharSequence
        get() = backingTextView.text

    val placeholder: CharSequence?
        get() = backingTextView.hint
}
