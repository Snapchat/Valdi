package com.snap.valdi.helloworld.textbinding

import android.content.Context
import com.snap.valdi.IValdiRuntime
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.logger.Logger

/**
 * Hands the fixture's attribute binders to the runtime, the way an app registers its own text
 * views at startup. A view whose binder is not registered has nothing bound and renders blank.
 */
object TextBindingFixtureBinders {

    fun register(
        runtime: IValdiRuntime,
        context: Context,
        fontManager: FontManager,
        defaultAttributes: FontAttributes,
        logger: Logger,
    ) {
        runtime.registerGlobalAttributesBinder(
            HelloWorldTextViewAttributesBinder(context, fontManager, defaultAttributes, logger)
        )
        runtime.registerGlobalAttributesBinder(
            HelloWorldEmojiTextViewAttributesBinder(context, fontManager, defaultAttributes, logger)
        )
    }
}
