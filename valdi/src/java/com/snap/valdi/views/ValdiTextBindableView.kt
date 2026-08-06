package com.snap.valdi.views

import android.widget.TextView
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.logger.Logger

/**
 * A view that renders Valdi text and can host a [TextViewHelper], exposing the concrete
 * [TextView] the helper draws into.
 *
 * Implemented by [ValdiTextViewBase] (which delegates to its inner backing TextView) and by
 * host-app text views that are not part of the ValdiTextViewBase hierarchy but still need to
 * receive Valdi text attributes (e.g. a platform font TextView). Lets a single attribute binder
 * ([com.snap.valdi.attributes.impl.AbstractTextViewAttributesBinder]) drive both.
 */
interface ValdiTextBindableView {

    /** The concrete TextView that renders the text (may be `this` or an inner backing view). */
    val bindingTextView: TextView

    fun getOrCreateTextViewHelper(
        fontManager: FontManager,
        defaultAttributes: FontAttributes,
        valueAttributeId: Int,
        logger: Logger,
    ): TextViewHelper
}
